#include "gravity/NBodySolver.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "sph/Diagnostics.h"
#include "system/Factory.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

NBodySolver::NBodySolver(const RunSettings& settings)
    : NBodySolver(settings, Factory::getGravity(settings)) {}

NBodySolver::NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
    : gravity(std::move(gravity))
    , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT)) {
    collision.handler = Factory::getCollisionHandler(settings);
    collision.finder = Factory::getFinder(settings);
    overlap.handler = Factory::getOverlapHandler(settings);
    overlap.allowedRatio = settings.get<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP);
}

NBodySolver::~NBodySolver() = default;

/*static void integrateRigidBody(Storage& storage, const Float dt) {
    ArrayView<Tensor> E = storage.getValue<Tensor>(QuantityId::LOCAL_FRAME);
    ArrayView<Vector> w, dw;
    tie(w, dw) = storage.getAll<Vector>(QuantityId::ANGULAR_VELOCITY);

    for (Size i = 0; i < w.size(); ++i) {
        //AffineMatrix w_cross = AffineMatrix::crossProductOperator(w[i]);
        // we need to convert to affine matrix, as the intermediate steps break the symmetry of the tensor
        //AffineMatrix Im = convert(I[i]);
        //AffineMatrix dIm = w_cross * Im - Im * w_cross;
        // convert back to the symmetric tensor, the result should be symmetric (checked by assert)
        //dI[i] = convert(dIm);
        // SymmetricTensor I0 = I[i];
        if (w[i] != Vector(0._f)) {
            Vector dir;
            Float omega;
            tieToTuple(dir, omega) = getNormalizedWithLength(w[i]);
            AffineMatrix rotation = AffineMatrix::rotateAxis(dir, omega * dt);
            AffineMatrix Em = convert<AffineMatrix>(E[i]);
            Em = rotation * Em * rotation.transpose();

            E[i] = convert<Tensor>(Em);
        }
        // SymmetricTensor Iinv = I[i].inverse();

        // using torque-free solution
        // I dw/dt + dI/dt * w = 0
        // dw[i] = -Iinv * (dI[i] * w[i]);
        // dw[i] = -Iinv * ((I[i] - I0) / dt * w[i]);
    }
}*/

void NBodySolver::integrate(Storage& storage, Statistics& stats) {
    gravity->build(storage);

    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    gravity->evalAll(pool, dv, stats);

    // null all derivatives of smoothing lengths (particle radii)
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < v.size(); ++i) {
        v[i][H] = 0._f;
        dv[i][H] = 0._f;
    }
}

void NBodySolver::collide(Storage& storage, Statistics& stats, const Float dt) {
    ArrayView<Vector> r, v, a;
    tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

    // find the radius of possible colliders
    const Float searchRadius = this->getSearchRadius(r, v, dt);

    // tree for finding collisions
    collision.finder->build(r);

    // handler determining collison outcomes
    collision.handler->initialize(storage);
    overlap.handler->initialize(storage);

    Size collisionCount = 0;
    Size mergerCount = 0;
    Size bounceCount = 0;
    Size overlapCount = 0;

    // holds computed collisions
    FlatSet<CollisionRecord> collisions;

    FlatSet<Size> invalidIdxs;

    // first pass - find all particles in interval <0, dt>
    /*for (Size i = 0; i < r.size(); ++i) {
        const Interval interval(0, dt);
        if (CollisionRecord col = this->findClosestCollision(i, searchRadius, interval, r, v)) {
            collisions.insert(col);
        }
    }

    Float time = 0._f;
    removed.clear();

    // go through all collisions, sorted by collision time
    while (!collisions.empty()) {
        const CollisionRecord& col = *collisions.begin();
        const Float t_coll = col.t_coll;
        const Size i = col.i;
        const Size j = col.j;

        // advance positions of collided particles to the collision time
        r[i] += v[i] * t_coll;
        r[j] += v[j] * t_coll;

        invalidIdx.clear();
        const CollisionResult result = collisionHandler->collide(i, j, removed);
        if (result != CollisionResult::NONE) {
            // move the time to the current collision time
            ASSERT(t_coll > time);
            time = t_coll;

            // remove all collisions containing either i or j
            for (auto iter = collisions.begin(); iter != collisions.end();) {
                const CollisionRecord& c = *iter;
                if (c.i == i || c.i == j || c.j == i || c.j == j) {
                    invalidIdx.insert(c.i);
                    invalidIdx.insert(c.j);
                    iter = collisions.erase(iter);
                } else {
                    ++iter;
                }
            }
        }

        // move particles back to the beginning of the time step
        r[i] -= v[i] * t_coll;
        r[j] -= v[j] * t_coll;
        collisionFinder->build(r);

        // find new collisions for invalid indices
        for (Size idx : invalidIdx) {
            if (removed.find(idx) != removed.end()) {
                // this particle has been removed, no need to find collisions
                continue;
            }
            const Interval interval(time, dt);
            if (CollisionRecord c = this->findClosestCollision(idx, searchRadius, interval, r, v)) {
                ASSERT((c.i != i || c.j != j) && (c.j != i || c.i != j));
                collisions.insert(c);
            }
        }
    }

    // advance all particles to the end of the timestep
    for (Size i = 0; i < r.size(); ++i) {
        r[i] += v[i] * dt;
    }

    // finally remove marked particles from the storage
    for (auto iter = removed.rbegin(); iter != removed.rend(); ++iter) {
        const Size i = *iter;
        ASSERT(i < storage.getParticleCnt(), i, storage.getParticleCnt());
        storage.remove(Array<Size>{ i });
    }
    storage.propagate([this](Storage& dependent) {
        for (auto iter = removed.rbegin(); iter != removed.rend(); ++iter) {
            dependent.remove(Array<Size>{ *iter });
        }
    });*/


    // holds indices of particles collisions of which have been invalidated and needs to be recomputed
    // std::set<Size> invalidIdx;

    // rigid body dynamics
    // integrateRigidBody(storage, stats.get<Float>(StatisticsId::TIMESTEP_VALUE));

    /* Float time = 0._f;
     while (true) {
         tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);
         switch (overlap.option) {
         case OverlapEnum::FORCE_MERGE:
             for (Size i = 0; i < r.size(); ++i) {
                 forceMerge(i, storage, searchRadius);
                 tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);
             }
             break;
         case OverlapEnum::NOTHING:
             break;
         default:
             NOT_IMPLEMENTED;
         }

         collisionFinder->build(r);

         CollisionRecord closestCollision;
         for (Size i = 0; i < r.size(); ++i) {
             if (CollisionRecord col = this->findClosestCollision(i, searchRadius, dt - time, neighs, r, v)) {
                 closestCollision = min(closestCollision, col);
             }
         }
         if (!closestCollision) {
             for (Size i = 0; i < r.size(); ++i) {
                 r[i] += v[i] * (dt - time);
             }
             break;
         }
         for (Size i = 0; i < r.size(); ++i) {
             r[i] += v[i] * closestCollision.t_coll;
         }
         Size i = closestCollision.i;
         Size j = closestCollision.j;
         toRemove.clear();
         const CollisionResult result = collisionHandler->collide(i, j, toRemove);
         if (result != CollisionResult::NONE) {
             time = closestCollision.t_coll;
         }

         if (!toRemove.empty()) {
             storage.remove(toRemove);
             // remove it also from all dependent storages, since this is a permanent action
             storage.propagate([&toRemove](Storage& dependent) { dependent.remove(toRemove); });

             // reset the invalidated arrayviews and rebuild the finder
             tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

             collisionFinder->build(r);
         }

         for (Size idx : toRemove) {
             if (idx < i) {
                 --i;
             }
         }
     }*/


    // first pass - find all collisions and sort them by collision time
    for (Size i = 0; i < r.size(); ++i) {
        if (CollisionRecord col = this->findClosestCollision(i, searchRadius, Interval(-LARGE, dt), r, v)) {
            ASSERT(isReal(col));
            collisions.insert(col);
        }
    }

    removed.clear();
    Float time = 0._f; // dt;
    while (!collisions.empty()) {
        const CollisionRecord& col = collisions[0];
        const Float t_coll = col.isOverlap() ? time : col.collisionTime;
        ASSERT(t_coll < dt);

        Size i = col.i;
        Size j = col.j;

        // advance all positions to the collision time
        if (t_coll != time) {
            for (Size i = 0; i < r.size(); ++i) {
                r[i] += v[i] * (t_coll - time);
                ASSERT(isReal(r[i]));
            }
        }

        // check and handle overlaps
        if (col.isOverlap()) {
            overlap.handler->collide(i, j, removed);
            overlapCount++;
        } else {
            const CollisionResult result = collision.handler->collide(i, j, removed);
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

        ASSERT(storage.isValid());
        time = t_coll; // move the time to the current collision time

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

        // find new collisions for invalid indices
        for (Size idx : invalidIdxs) {
            // here we shouldn't search any removed particle
            if (removed.find(idx) != removed.end()) {
                continue;
            }
            if (CollisionRecord c =
                    this->findClosestCollision(idx, searchRadius, Interval(-LARGE, dt - time), r, v)) {
                ASSERT(isReal(c));
                ASSERT(removed.find(c.i) == removed.end() && removed.find(c.j) == removed.end());
                if ((c.i == i && c.j == j) || (c.j == i && c.i == j)) {
                    // don't process the same pair twice in a row
                    continue;
                }
                c.collisionTime += time; // ok to advance time for overlaps, it is never used
                collisions.insert(c);
            }
        }
    }
    // advance all positions by the remaining time
    for (Size i = 0; i < r.size(); ++i) {
        r[i] += v[i] * (dt - time);
    }

    // apply the removal list
    if (!removed.empty()) {
        storage.remove(removed);
        // remove it also from all dependent storages, since this is a permanent action
        // storage.propagate([this](Storage& dependent) { dependent.remove(removed); });
    }

    stats.set(StatisticsId::TOTAL_COLLISION_COUNT, int(collisionCount));
    stats.set(StatisticsId::BOUNCE_COUNT, int(bounceCount));
    stats.set(StatisticsId::MERGER_COUNT, int(mergerCount));
    stats.set(StatisticsId::OVERLAP_COUNT, int(overlapCount));
}

void NBodySolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    storage.insert<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, SymmetricTensor::null());
    ArrayView<SymmetricTensor> I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    for (Size i = 0; i < r.size(); ++i) {
        I[i] = Rigid::sphereInertia(m[i], r[i][H]);
    }
    storage.insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST, Vector(0._f));

    // zero order, we integrate the frame coordinates manually
    storage.insert<Tensor>(QuantityId::LOCAL_FRAME, OrderEnum::ZERO, Tensor::null());
}

Float NBodySolver::getSearchRadius(ArrayView<const Vector> r, ArrayView<const Vector> v, const Float dt) {
    // find the largest velocity, so that we know how far to search for potentional impactors
    /// \todo naive implementation, improve
    Float v_max = 0._f;
    Float h_max = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        v_max = max(v_max, getSqrLength(v[i]));
        h_max = max(h_max, r[i][H]);
    }
    v_max = sqrt(v_max);
    return v_max * dt + h_max;
}

CollisionRecord NBodySolver::findClosestCollision(const Size i,
    const Float globalRadius,
    const Interval interval,
    ArrayView<Vector> r,
    ArrayView<Vector> v) {
    // maximum travel of i-th particle
    const Float localRadius = r[i][H] + getLength(v[i]) * interval.upper();
    // since we don't know a priori the travel distance of neighbours, use the maximum of all
    collision.finder->findNeighbours(i, globalRadius + localRadius, neighs, EMPTY_FLAGS);

    CollisionRecord closestCollision;
    for (NeighbourRecord& n : neighs) {
        const Size j = n.index;
        if (j == i || removed.find(j) != removed.end()) {
            // either the particle itself or particle already removed, skip
            continue;
        }
        const Float overlapValue = 1._f - getSqrLength(r[i] - r[j]) / sqr(r[i][H] + r[j][H]);
        if (overlapValue > sqr(overlap.allowedRatio)) {
            // make as overlap
            return CollisionRecord::OVERLAP(i, j, overlapValue);
        }

        Optional<Float> t_coll = this->checkCollision(r[i], v[i], r[j], v[j], interval);
        if (t_coll) {
            ASSERT(interval.contains(t_coll.value()));
            closestCollision = min(closestCollision, CollisionRecord::COLLISION(i, j, t_coll.value()));
        }
    }
    return closestCollision;
}

Optional<Float> NBodySolver::checkCollision(const Vector& r1,
    const Vector& v1,
    const Vector& r2,
    const Vector& v2,
    const Interval interval) const {
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
        // collision time can be slightly negative number, corresponding to collision of overlaping
        // particles.
        ASSERT(isReal(t_coll) && t_coll >= 0._f);

        // collision happens if the collision time is less than the current timestep
        if (interval.contains(t_coll)) {
            return t_coll;
        }
    }
    return NOTHING;
}

NAMESPACE_SPH_END
