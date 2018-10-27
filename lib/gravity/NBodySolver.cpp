#include "gravity/NBodySolver.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "objects/finders/NeighbourFinder.h"
#include "sph/Diagnostics.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "system/Timer.h"

NAMESPACE_SPH_BEGIN

NBodySolver::NBodySolver(IScheduler& scheduler, const RunSettings& settings)
    : NBodySolver(scheduler, settings, Factory::getGravity(settings)) {}

NBodySolver::NBodySolver(IScheduler& scheduler, const RunSettings& settings, AutoPtr<IGravity>&& gravity)
    : NBodySolver(scheduler,
          settings,
          std::move(gravity),
          Factory::getCollisionHandler(settings),
          Factory::getOverlapHandler(settings)) {}

NBodySolver::NBodySolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IGravity>&& gravity,
    AutoPtr<ICollisionHandler>&& collisionHandler,
    AutoPtr<IOverlapHandler>&& overlapHandler)
    : gravity(std::move(gravity))
    , scheduler(scheduler)
    , threadData(scheduler) {
    collision.handler = std::move(collisionHandler);
    collision.finder = Factory::getFinder(settings);
    overlap.handler = std::move(overlapHandler);
    overlap.allowedRatio = settings.get<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP);
    rigidBody.use = settings.get<bool>(RunSettingsId::NBODY_INERTIA_TENSOR);
    rigidBody.maxAngle = settings.get<Float>(RunSettingsId::NBODY_MAX_ROTATION_ANGLE);
}

NBodySolver::~NBodySolver() = default;

/*static void integrateOmega(Storage& storage) {
    ArrayView<SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    ArrayView<Tensor> E = storage.getValue<Tensor>(QuantityId::LOCAL_FRAME);
    ArrayView<Vector> w, dw;
    tie(w, dw) = storage.getAll<Vector>(QuantityId::ANGULAR_VELOCITY);

    for (Size i = 0; i < w.size(); ++i) {
        if (w[i] == Vector(0._f)) {
            continue;
        }
        SymmetricTensor Iinv = I[i].inverse();
        // convert the angular frequency to the local frame;
        // note the E is always orthogonal, so we can use transpose instead of inverse
        AffineMatrix Em = convert<AffineMatrix>(E[i]);
        ASSERT(Em.isOrthogonal());
        const Vector w_loc = Em.transpose() * w[i];

        // compute the change of angular velocity using Euler's equations
        const Vector dw_loc = -Iinv * cross(w_loc, I[i] * w_loc);

        // now dw[i] is also in local space, convert to inertial space
        // (note that dw_in = dw_loc as omega x omega is obviously zero)
        dw[i] = Em * dw_loc;
    }
}*/

void NBodySolver::rotateLocalFrame(Storage& storage, const Float dt) {
    ArrayView<Tensor> E = storage.getValue<Tensor>(QuantityId::LOCAL_FRAME);
    ArrayView<Vector> L = storage.getValue<Vector>(QuantityId::ANGULAR_MOMENTUM);
    ArrayView<Vector> w = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
    ArrayView<SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);

    for (Size i = 0; i < L.size(); ++i) {
        if (L[i] == Vector(0._f)) {
            continue;
        }
        AffineMatrix Em = convert<AffineMatrix>(E[i]);

        const Float omega = getLength(w[i]);
        const Float dphi = omega * dt;

        if (almostEqual(I[0], SymmetricTensor(Vector(I[0].trace() / 3._f), Vector(0._f)), 1.e-6_f)) {
            // (almost) isotropic particle, we can skip the substepping and omega integration
            const Vector dir = getNormalized(w[i]);
            AffineMatrix rotation = AffineMatrix::rotateAxis(dir, dphi);
            ASSERT(Em.isOrthogonal());
            E[i] = convert<Tensor>(rotation * Em);
            continue;
        }

        // To ensure we never rotate more than maxAngle, we do a 'substepping' of angular velocity here;
        // rotate the local frame around the current omega by maxAngle, compute the new omega, and so on,
        // until we rotated by dphi
        // To disable it, just set maxAngle to very high value. Note that nothing gets 'broken'; both angular
        // momentum and moment of inertia are always conserved (by construction), but the precession might not
        // be solved correctly
        for (Float totalRot = 0._f; totalRot < dphi; totalRot += rigidBody.maxAngle) {
            const Vector dir = getNormalized(w[i]);

            const Float rot = min(rigidBody.maxAngle, dphi - totalRot);
            AffineMatrix rotation = AffineMatrix::rotateAxis(dir, rot);

            ASSERT(Em.isOrthogonal());
            Em = rotation * Em;

            // compute new angular velocity, to keep angular velocity consistent with angular momentum
            // note that this assumes that L and omega are set up consistently
            const SymmetricTensor I_in = transform(I[i], Em);
            const SymmetricTensor I_inv = I_in.inverse();
            w[i] = I_inv * L[i];
        }
        E[i] = convert<Tensor>(Em);
    }
}

void NBodySolver::integrate(Storage& storage, Statistics& stats) {
    Timer timer;
    gravity->build(scheduler, storage);

    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    gravity->evalAll(scheduler, dv, stats);

    // null all derivatives of smoothing lengths (particle radii)
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < v.size(); ++i) {
        v[i][H] = 0._f;
        dv[i][H] = 0._f;
    }
    stats.set(StatisticsId::GRAVITY_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));

    /*if (useInertiaTensor) {
        integrateOmega(storage);
    } else {
        // do nothing, for spherical particles the omega is constant
    }*/
}

class CollisionStats {
private:
    Statistics& stats;

public:
    /// Number of all collisions (does not count overlaps)
    Size collisionCount = 0;

    /// Out of all collisions, how many mergers
    Size mergerCount = 0;

    /// Out of all collisions, how many bounces
    Size bounceCount = 0;

    /// Number of overlaps handled
    Size overlapCount = 0;

    CollisionStats(Statistics& stats)
        : stats(stats) {}

    ~CollisionStats() {
        stats.set(StatisticsId::TOTAL_COLLISION_COUNT, int(collisionCount));
        stats.set(StatisticsId::BOUNCE_COUNT, int(bounceCount));
        stats.set(StatisticsId::MERGER_COUNT, int(mergerCount));
        stats.set(StatisticsId::OVERLAP_COUNT, int(overlapCount));
    }

    void clasify(const CollisionResult result) {
        collisionCount++;
        switch (result) {
        case CollisionResult::BOUNCE:
            bounceCount++;
            break;
        case CollisionResult::MERGER:
            mergerCount++;
            break;
        case CollisionResult::NONE:
            // do nothing
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }
};


struct CollisionRecord {
    /// Indices of the collided particles.
    Size i;
    Size j;

    Float collisionTime = INFINITY;
    Float overlap = 0._f;

    CollisionRecord() = default;

    CollisionRecord(const Size i, const Size j, const Float overlap, const Float time)
        : i(i)
        , j(j)
        , collisionTime(time)
        , overlap(overlap) {}

    bool operator<(const CollisionRecord& other) const {
        return std::make_tuple(collisionTime, -overlap, i, j) <
               std::make_tuple(other.collisionTime, -other.overlap, other.i, other.j);
    }

    /// Returns true if there is some collision or overlap
    explicit operator bool() const {
        return overlap > 0._f || collisionTime < INFINITY;
    }

    static CollisionRecord COLLISION(const Size i, const Size j, const Float time) {
        return CollisionRecord(i, j, 0._f, time);
    }

    static CollisionRecord OVERLAP(const Size i, const Size j, const Float time, const Float overlap) {
        return CollisionRecord(i, j, overlap, time);
    }

    bool isOverlap() const {
        return overlap > 0._f;
    }

    friend bool isReal(const CollisionRecord& col) {
        return col.isOverlap() ? isReal(col.overlap) : isReal(col.collisionTime);
    }
};

void NBodySolver::collide(Storage& storage, Statistics& stats, const Float dt) {
    Timer timer;
    if (rigidBody.use) {
        rotateLocalFrame(storage, dt);
    }

    ArrayView<Vector> a;
    tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

    // find the radius of possible colliders
    // const Float searchRadius = getSearchRadius(r, v, dt);

    // tree for finding collisions
    collision.finder->buildWithRank(SEQUENTIAL, r, [this, dt](const Size i, const Size j) {
        return r[i][H] + getLength(v[i]) * dt < r[j][H] + getLength(v[j]) * dt;
    });

    // handler determining collision outcomes
    collision.handler->initialize(storage);
    overlap.handler->initialize(storage);

    // ignored.clear();
    collisions.clear();
    searchRadii.resize(r.size());
    searchRadii.fill(0._f);

    for (ThreadData& data : threadData) {
        data.collisions.clear();
    }

    // first pass - find all collisions and sort them by collision time
    parallelFor(scheduler, threadData, 0, r.size(), [&](const Size i, ThreadData& data) {
        if (CollisionRecord col =
                this->findClosestCollision(i, SearchEnum::FIND_LOWER_RANK, Interval(0._f, dt), data.neighs)) {
            ASSERT(isReal(col));
            data.collisions.insert(col);
        }
    });
    for (ThreadData& data : threadData) {
        for (auto& col : data.collisions) {
            collisions.insert(col);
        }
    }

    CollisionStats cs(stats);
    removed.clear();

    /// \todo
    /// We have to process the all collision in order, sorted according to collision time, but this is
    /// hardly parallelized. We can however process collisions concurrently, as long as the collided
    /// particles don't intersect the spheres with radius equal to the search radius. Note that this works
    /// as long as the search radius does not increase during collision handling.

    FlatSet<Size> invalidIdxs;
    while (!collisions.empty()) {
        const CollisionRecord& col = *collisions.begin();
        const Float t_coll = col.collisionTime;
        ASSERT(t_coll < dt);

        Size i = col.i;
        Size j = col.j;

        // advance the positions of collided particles to the collision time
        r[i] += v[i] * t_coll;
        r[j] += v[j] * t_coll;
        ASSERT(isReal(r[i]) && isReal(r[j]));

        // check and handle overlaps
        CollisionResult result;
        if (col.isOverlap()) {
            overlap.handler->handle(i, j, removed);
            result = CollisionResult::BOUNCE; ///\todo
            cs.overlapCount++;
        } else {
            result = collision.handler->collide(i, j, removed);
            cs.clasify(result);
        }

        // move the positions back to the beginning of the timestep
        r[i] -= v[i] * t_coll;
        r[j] -= v[j] * t_coll;
        ASSERT(isReal(r[i]) && isReal(r[j]));

        if (result == CollisionResult::NONE) {
            // no collision to process
            collisions.erase(collisions.begin());
            continue;
        }

        // remove all collisions containing either i or j
        invalidIdxs.clear();
        for (auto iter = collisions.begin(); iter != collisions.end();) {
            const CollisionRecord& c = *iter;
            if (c.i == i || c.i == j || c.j == i || c.j == j) {
                invalidIdxs.insert(c.i);
                invalidIdxs.insert(c.j);
                iter = collisions.erase(iter);
            } else {
                ++iter;
            }
        }

        for (Size idx : invalidIdxs) {
            // here we shouldn't search any removed particle
            if (removed.find(idx) != removed.end()) {
                continue;
            }
            const Interval interval(t_coll, dt);
            if (CollisionRecord c =
                    this->findClosestCollision(idx, SearchEnum::USE_RADII, interval, neighs)) {
                ASSERT(isReal(c));
                ASSERT(removed.find(c.i) == removed.end() && removed.find(c.j) == removed.end());
                if ((c.i == i && c.j == j) || (c.j == i && c.i == j)) {
                    // don't process the same pair twice in a row
                    continue;
                }
                collisions.insert(c);
            }
        }
    }

    // apply the removal list
    if (!removed.empty()) {
        storage.remove(removed, Storage::RemoveFlag::INDICES_SORTED);
        // remove it also from all dependent storages, since this is a permanent action
        storage.propagate([this](Storage& dependent) { dependent.remove(removed); });
    }
    ASSERT(storage.isValid());

    stats.set(StatisticsId::COLLISION_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

void NBodySolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    storage.insert<Vector>(QuantityId::ANGULAR_MOMENTUM, OrderEnum::ZERO, Vector(0._f));

    // dependent quantity, computed from angular momentum
    storage.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

    if (rigidBody.use) {
        storage.insert<SymmetricTensor>(
            QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, SymmetricTensor::null());
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
        for (Size i = 0; i < r.size(); ++i) {
            I[i] = Rigid::sphereInertia(m[i], r[i][H]);
        }

        // zero order, we integrate the frame coordinates manually
        storage.insert<Tensor>(QuantityId::LOCAL_FRAME, OrderEnum::ZERO, Tensor::identity());
    }
}

CollisionRecord NBodySolver::findClosestCollision(const Size i,
    const SearchEnum opt,
    const Interval interval,
    Array<NeighbourRecord>& neighs) {
    ASSERT(!interval.empty());
    if (opt == SearchEnum::FIND_LOWER_RANK) {
        // maximum travel of i-th particle
        const Float radius = r[i][H] + getLength(v[i]) * interval.upper();
        collision.finder->findLowerRank(i, 2._f * radius, neighs);
    } else {
        ASSERT(isReal(searchRadii[i]));
        if (searchRadii[i] > 0._f) {
            collision.finder->findAll(i, 2._f * searchRadii[i], neighs);
        } else {
            return CollisionRecord{};
        }
    }

    CollisionRecord closestCollision;
    for (NeighbourRecord& n : neighs) {
        const Size j = n.index;
        if (opt == SearchEnum::FIND_LOWER_RANK) {
            searchRadii[i] = searchRadii[j] = r[i][H] + getLength(v[i]) * interval.upper();
        }
        if (i == j || removed.find(j) != removed.end()) {
            // particle already removed, skip
            continue;
        }
        // advance positions to the start of the interval
        const Vector r1 = r[i] + v[i] * interval.lower();
        const Vector r2 = r[j] + v[j] * interval.lower();
        const Float overlapValue = 1._f - getSqrLength(r1 - r2) / sqr(r[i][H] + r[j][H]);
        if (overlapValue > sqr(overlap.allowedRatio)) {
            if (overlap.handler->overlaps(i, j)) {
                // this overlap needs to be handled
                return CollisionRecord::OVERLAP(i, j, interval.lower(), overlapValue);
            } else {
                // skip this overlap, which also implies skipping the collision, so just continue
                continue;
            }
        }

        Optional<Float> t_coll = this->checkCollision(r1, v[i], r2, v[j], interval.size());
        if (t_coll) {
            // t_coll is relative to the interval, convert to timestep 'coordinates'
            const Float time = t_coll.value() + interval.lower();
            closestCollision = min(closestCollision, CollisionRecord::COLLISION(i, j, time));
        }
    }
    return closestCollision;
}

Optional<Float> NBodySolver::checkCollision(const Vector& r1,
    const Vector& v1,
    const Vector& r2,
    const Vector& v2,
    const Float dt) const {
    const Vector dr = r1 - r2;
    const Vector dv = v1 - v2;
    const Float dvdr = dot(dv, dr);
    if (dvdr >= 0._f) {
        // not moving towards each other, no collision
        return NOTHING;
    }

    const Vector dr_perp = dr - dot(dv, dr) * dv / getSqrLength(dv);
    ASSERT(getSqrLength(dr_perp) < (1._f + EPS) * getSqrLength(dr), dr_perp, dr);
    if (getSqrLength(dr_perp) <= sqr(r1[H] + r2[H])) {
        // on collision trajectory, find the collision time
        const Float dv2 = getSqrLength(dv);
        const Float det = 1._f - (getSqrLength(dr) - sqr(r1[H] + r2[H])) / sqr(dvdr) * dv2;
        // ASSERT(det >= 0._f);
        const Float sqrtDet = sqrt(max(0._f, det));
        const Float root = (det > 1._f) ? 1._f + sqrtDet : 1._f - sqrtDet;
        const Float t_coll = -dvdr / dv2 * root;
        ASSERT(isReal(t_coll) && t_coll >= 0._f);

        // t_coll can never be negative (which we check by assert), so only check if it is lower than the
        // interval size
        if (t_coll <= dt) {
            return t_coll;
        }
    }
    return NOTHING;
}

NAMESPACE_SPH_END
