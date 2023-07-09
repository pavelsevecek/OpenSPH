#include "gravity/NBodySolver.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "io/Logger.h"
#include "objects/finders/NeighborFinder.h"
#include "quantities/Quantity.h"
#include "sph/Diagnostics.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include <map>
#include <set>

NAMESPACE_SPH_BEGIN

HardSphereSolver::HardSphereSolver(IScheduler& scheduler, const RunSettings& settings)
    : HardSphereSolver(scheduler, settings, Factory::getGravity(settings)) {}

HardSphereSolver::HardSphereSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IGravity>&& gravity)
    : HardSphereSolver(scheduler,
          settings,
          std::move(gravity),
          Factory::getCollisionHandler(settings),
          Factory::getOverlapHandler(settings)) {}

HardSphereSolver::HardSphereSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IGravity>&& gravity,
    AutoPtr<ICollisionHandler>&& collisionHandler,
    AutoPtr<IOverlapHandler>&& overlapHandler)
    : gravity(std::move(gravity))
    , scheduler(scheduler)
    , threadData(scheduler) {
    collision.handler = std::move(collisionHandler);
    collision.finder = Factory::getFinder(settings);
    collision.maxBounces = settings.get<int>(RunSettingsId::COLLISION_MAX_BOUNCES);
    overlap.handler = std::move(overlapHandler);
    overlap.allowedRatio = settings.get<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP);
    rigidBody.use = settings.get<bool>(RunSettingsId::NBODY_INERTIA_TENSOR);
    rigidBody.maxAngle = settings.get<Float>(RunSettingsId::NBODY_MAX_ROTATION_ANGLE);
}

HardSphereSolver::~HardSphereSolver() = default;

void HardSphereSolver::rotateLocalFrame(Storage& storage, const Float dt) {
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
            SPH_ASSERT(Em.isOrthogonal());
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

            SPH_ASSERT(Em.isOrthogonal());
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

void HardSphereSolver::integrate(Storage& storage, Statistics& stats) {
    VERBOSE_LOG;

    Timer timer;
    gravity->build(scheduler, storage);

    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    SPH_ASSERT_UNEVAL(std::all_of(dv.begin(), dv.end(), [](const Vector& a) { return a == Vector(0._f); }));
    gravity->evalSelfGravity(scheduler, dv, stats);

    ArrayView<Attractor> attractors = storage.getAttractors();
    gravity->evalAttractors(scheduler, attractors, dv);

    // null all derivatives of smoothing lengths (particle radii)
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < v.size(); ++i) {
        v[i][H] = 0._f;
        dv[i][H] = 0._f;
    }
    stats.set(StatisticsId::GRAVITY_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
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

    explicit CollisionStats(Statistics& stats)
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
    Size i = Size(-1);
    Size j = Size(-1);

    Float collisionTime = INFINITY;
    Float overlap = 0._f;

    CollisionRecord() = default;

    CollisionRecord(const Size i, const Size j, const Float overlap, const Float time)
        : i(i)
        , j(j)
        , collisionTime(time)
        , overlap(overlap) {}

    bool operator==(const CollisionRecord& other) const {
        return i == other.i && j == other.j && collisionTime == other.collisionTime &&
               overlap == other.overlap;
    }

    bool operator<(const CollisionRecord& other) const {
        return std::make_tuple(collisionTime, -overlap, i, j) <
               std::make_tuple(other.collisionTime, -other.overlap, other.i, other.j);
    }

    /// Returns true if there is some collision or overlap
    explicit operator bool() const {
        return overlap > 0._f || collisionTime < INFTY;
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

class CollisionSet {
public:
    using Iterator = std::set<CollisionRecord>::const_iterator;

private:
    // holds all collisions
    std::set<CollisionRecord> collisions;

    // maps particles to the collisions
    std::multimap<Size, CollisionRecord> indexToCollision;

    using Itc = std::multimap<Size, CollisionRecord>::const_iterator;

public:
    void insert(const CollisionRecord& col) {
        Iterator iter;
        bool inserted;
        std::tie(iter, inserted) = collisions.insert(col);
        if (!inserted) {
            return;
        }
        indexToCollision.insert(std::make_pair(col.i, col));
        indexToCollision.insert(std::make_pair(col.j, col));
    }

    template <typename TIter>
    void insert(TIter first, TIter last) {
        for (TIter iter = first; iter != last; ++iter) {
            insert(*iter);
        }
        checkConsistency();
    }

    const CollisionRecord& top() const {
        return *collisions.begin();
    }

    bool empty() const {
        SPH_ASSERT(collisions.empty() == indexToCollision.empty());
        return collisions.empty();
    }

    bool has(const Size idx) const {
        return indexToCollision.count(idx) > 0;
    }

    void removeByCollision(const CollisionRecord& col) {
        removeIndex(col, col.i);
        removeIndex(col, col.j);
        const Size removed = Size(collisions.erase(col));
        SPH_ASSERT(removed == 1);
        checkConsistency();
    }

    void removeByIndex(const Size idx, FlatSet<Size>& removed) {
        Itc first, last;
        removed.insert(idx);
        std::tie(first, last) = indexToCollision.equal_range(idx);
        // last iterator may be invalidated, so we need to be careful
        for (Itc itc = first; itc != indexToCollision.end() && itc->first == idx;) {
            const Size otherIdx = (itc->second.i == idx) ? itc->second.j : itc->second.i;
            removed.insert(otherIdx);
            collisions.erase(itc->second);
            // erase the other particle as well
            removeIndex(itc->second, otherIdx);
            itc = indexToCollision.erase(itc);
        }
        checkConsistency();
    }

private:
    void removeIndex(const CollisionRecord& col, const Size idx) {
        SPH_ASSERT(col.i == idx || col.j == idx);
        Itc first, last;
        std::tie(first, last) = indexToCollision.equal_range(idx);
        for (Itc itc = first; itc != last; ++itc) {
            if (itc->second == col) {
                indexToCollision.erase(itc);
                return;
            }
        }
        SPH_ASSERT(false, "Collision not found");
    }

#ifdef SPH_DEBUG
    void checkConsistency() const {
        SPH_ASSERT(2 * collisions.size() == indexToCollision.size());
        // all collisions are in the index-to-colision map
        for (const CollisionRecord& col : collisions) {
            SPH_ASSERT(indexToCollision.count(col.i));
            SPH_ASSERT(indexToCollision.count(col.j));
            SPH_ASSERT(hasCollision(col, col.i));
            SPH_ASSERT(hasCollision(col, col.j));
        }

        // all entries in index-to-collision map have a corresponding collision
        for (const auto& p : indexToCollision) {
            SPH_ASSERT(collisions.find(p.second) != collisions.end());
        }
    }
#else
    void checkConsistency() const {}
#endif

    bool hasCollision(const CollisionRecord& col, const Size idx) const {
        Itc first, last;
        std::tie(first, last) = indexToCollision.equal_range(idx);
        for (Itc itc = first; itc != last; ++itc) {
            if (itc->second == col) {
                return true;
            }
        }
        return false;
    }
};

void HardSphereSolver::collide(Storage& storage, Statistics& stats, const Float dt) {
    VERBOSE_LOG

    if (!collision.handler) {
        // ignore all collisions
        return;
    }

    Timer timer;
    if (rigidBody.use) {
        rotateLocalFrame(storage, dt);
    }

    ArrayView<Vector> a;
    tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

    // find the radius of possible colliders
    // const Float searchRadius = getSearchRadius(r, v, dt);

    // tree for finding collisions
    collision.finder->buildWithRank(scheduler, r, [this, dt](const Size i, const Size j) {
        return r[i][H] + getLength(v[i]) * dt < r[j][H] + getLength(v[j]) * dt;
    });

    // handler determining collision outcomes
    collision.handler->initialize(storage);
    overlap.handler->initialize(storage);

    searchRadii.resize(r.size());
    searchRadii.fill(0._f);

    numBounces.resize(r.size());
    numBounces.fill(0);

    for (ThreadData& data : threadData) {
        data.collisions.clear();
    }

    // first pass - find all collisions and sort them by collision time
    parallelFor(scheduler, threadData, 0, r.size(), [&](const Size i, ThreadData& data) {
        if (CollisionRecord col =
                this->findClosestCollision(i, SearchEnum::FIND_LOWER_RANK, Interval(0._f, dt), data.neighs)) {
            SPH_ASSERT(isReal(col));
            data.collisions.push(col);
        }
    });

    // Holds all detected collisions.
    CollisionSet collisions;

    // reduce thread-local containers
    {
        ThreadData* main = nullptr;
        for (ThreadData& data : threadData) {
            if (!main) {
                main = &data;
            } else {
                main->collisions.pushAll(data.collisions);
                data.collisions.clear();
            }
        }
        // sort to get a deterministic order in index-to-collision maps
        std::sort(main->collisions.begin(), main->collisions.end());
        collisions.insert(main->collisions.begin(), main->collisions.end());
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
        // find first collision in the list
        const CollisionRecord& col = collisions.top();
        const Float t_coll = col.collisionTime;
        SPH_ASSERT(t_coll < dt);

        Size i = col.i;
        Size j = col.j;

        // advance the positions of collided particles to the collision time
        r[i] += v[i] * t_coll;
        r[j] += v[j] * t_coll;
        SPH_ASSERT(isReal(r[i]) && isReal(r[j]));

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
        SPH_ASSERT(isReal(r[i]) && isReal(r[j]));

        if (result == CollisionResult::NONE) {
            // no collision to process
            collisions.removeByCollision(col);
            continue;
        }

        // remove all collisions containing either i or j
        invalidIdxs.clear();
        collisions.removeByIndex(i, invalidIdxs);
        collisions.removeByIndex(j, invalidIdxs);
        SPH_ASSERT(!collisions.has(i));
        SPH_ASSERT(!collisions.has(j));

        numBounces[i]++;
        numBounces[j]++;

        const Interval interval(t_coll + EPS, dt);
        if (!interval.empty()) {
            for (Size idx : invalidIdxs) {
                // here we shouldn't search any removed particle
                if (removed.find(idx) != removed.end()) {
                    continue;
                }
                if (numBounces[idx] > collision.maxBounces) {
                    // limit reached
                    continue;
                }
                if (CollisionRecord c =
                        this->findClosestCollision(idx, SearchEnum::USE_RADII, interval, neighs)) {
                    SPH_ASSERT(isReal(c));
                    SPH_ASSERT(removed.find(c.i) == removed.end() && removed.find(c.j) == removed.end());
                    if ((c.i == i && c.j == j) || (c.j == i && c.i == j)) {
                        // don't process the same pair twice in a row
                        continue;
                    }

                    collisions.insert(c);
                }
            }
        }
    }

    // apply the removal list
    if (!removed.empty()) {
        // remove it also from all dependent storages, since this is a permanent action
        storage.remove(removed, Storage::IndicesFlag::INDICES_SORTED | Storage::IndicesFlag::PROPAGATE);
    }
    SPH_ASSERT(storage.isValid());

    stats.set(StatisticsId::COLLISION_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

void HardSphereSolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    VERBOSE_LOG

    // dependent quantity, computed from angular momentum
    storage.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

    if (rigidBody.use) {
        storage.insert<Vector>(QuantityId::ANGULAR_MOMENTUM, OrderEnum::ZERO, Vector(0._f));
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

CollisionRecord HardSphereSolver::findClosestCollision(const Size i,
    const SearchEnum opt,
    const Interval interval,
    Array<NeighborRecord>& neighs) {
    SPH_ASSERT(!interval.empty());
    if (opt == SearchEnum::FIND_LOWER_RANK) {
        // maximum travel of i-th particle
        const Float radius = r[i][H] + getLength(v[i]) * interval.upper();
        collision.finder->findLowerRank(i, 2._f * radius, neighs);
    } else {
        SPH_ASSERT(isReal(searchRadii[i]));
        if (searchRadii[i] > 0._f) {
            collision.finder->findAll(i, 2._f * searchRadii[i], neighs);
        } else {
            return CollisionRecord{};
        }
    }

    CollisionRecord closestCollision;
    for (NeighborRecord& n : neighs) {
        const Size j = n.index;
        if (opt == SearchEnum::FIND_LOWER_RANK) {
            searchRadii[i] = searchRadii[j] = r[i][H] + getLength(v[i]) * interval.upper();
        }
        if (i == j || removed.find(j) != removed.end()) {
            // particle already removed, skip
            continue;
        }
        if (numBounces[j] > collision.maxBounces) {
            // limit reached
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

Optional<Float> HardSphereSolver::checkCollision(const Vector& r1,
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
    SPH_ASSERT(getSqrLength(dr_perp) < (1._f + EPS) * getSqrLength(dr), dr_perp, dr);
    if (getSqrLength(dr_perp) <= sqr(r1[H] + r2[H])) {
        // on collision trajectory, find the collision time
        const Float dv2 = getSqrLength(dv);
        const Float det = 1._f - (getSqrLength(dr) - sqr(r1[H] + r2[H])) / sqr(dvdr) * dv2;
        // SPH_ASSERT(det >= 0._f);
        const Float sqrtDet = sqrt(max(0._f, det));
        const Float root = (det > 1._f) ? 1._f + sqrtDet : 1._f - sqrtDet;
        const Float t_coll = -dvdr / dv2 * root;
        SPH_ASSERT(isReal(t_coll) && t_coll >= 0._f);

        // t_coll can never be negative (which we check by assert), so only check if it is lower than the
        // interval size
        if (t_coll <= dt) {
            return t_coll;
        }
    }
    return NOTHING;
}

SoftSphereSolver::SoftSphereSolver(IScheduler& scheduler, const RunSettings& settings)
    : SoftSphereSolver(scheduler, settings, Factory::getGravity(settings)) {}

SoftSphereSolver::SoftSphereSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IGravity>&& gravity)
    : gravity(std::move(gravity))
    , scheduler(scheduler)
    , threadData(scheduler) {
    finder = Factory::getFinder(settings);
    springConstant = settings.get<Float>(RunSettingsId::NBODY_SOFTSPHERE_SPRING_CONSTANT);
    epsilon = settings.get<Float>(RunSettingsId::NBODY_SOFTSPHERE_RESTITUTION_COEFFICIENT);

    h1 = sqr(PI);
    h2 = 2 * PI / sqrt(sqr(PI / log(epsilon)) + 1);
}

SoftSphereSolver::~SoftSphereSolver() = default;

void SoftSphereSolver::integrate(Storage& storage, Statistics& stats) {
    VERBOSE_LOG;

    Timer timer;
    gravity->build(scheduler, storage);

    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    SPH_ASSERT_UNEVAL(std::all_of(dv.begin(), dv.end(), [](const Vector& a) { return a == Vector(0._f); }));
    gravity->evalSelfGravity(scheduler, dv, stats);

    ArrayView<Attractor> attractors = storage.getAttractors();
    gravity->evalAttractors(scheduler, attractors, dv);

    stats.set(StatisticsId::GRAVITY_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
    timer.restart();

    if (RawPtr<const IBasicFinder> gravityFinder = gravity->getFinder()) {
        evalCollisions(storage, *gravityFinder);
    } else {
        finder->build(scheduler, r);
        evalCollisions(storage, *finder);
    }

    stats.set(StatisticsId::COLLISION_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

inline Float orbitTime(Float mass, Float a, Float G = Constants::gravity) {
    const Float rhs = (G * mass) / (4 * sqr(PI));
    return sqrt(pow<3>(a) / rhs);
}

void SoftSphereSolver::evalCollisions(Storage& storage, const IBasicFinder& finder) {
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    Float searchRadius = 0;
    for (Size i = 0; i < r.size(); ++i) {
        searchRadius = max(searchRadius, 2 * r[i][H]);
    }

    parallelFor(scheduler, threadData, 0, r.size(), [&](const Size i, ThreadData& data) {
        finder.findAll(r[i], searchRadius, data.neighs);
        for (const auto& n : data.neighs) {
            const Size j = n.index;
            if (i == j || n.distanceSqr >= sqr(r[i][H] + r[j][H])) {
                continue;
            }

            Vector dir;
            Float dist;
            tieToTuple(dir, dist) = getNormalizedWithLength(r[j] - r[i]);
            const Float alpha = r[i][H] + r[j][H] - dist;
            SPH_ASSERT(alpha >= 0);
            const Vector delta_v = v[j] - v[i];
            const Float alpha_dot = -dot(delta_v, dir);
            const Float m_eff = (m[i] * m[j]) / (m[i] + m[j]);
            const Float t_dur = springConstant * orbitTime(m[i] + m[j], r[i][H] + r[j][H]);
            const Float k1 = m_eff * h1 / sqr(t_dur);
            const Float k2 = m_eff * h2 / t_dur;
            const Vector force = (k1 * alpha + k2 * alpha_dot) * dir;
            dv[i] -= force / m[i];
        }
        dv[i][H] = 0;
    });
}

void SoftSphereSolver::create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const {}

NAMESPACE_SPH_END
