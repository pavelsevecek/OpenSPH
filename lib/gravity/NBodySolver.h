#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "gravity/IGravity.h"
#include "objects/finders/BruteForceFinder.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "thread/Pool.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// TODO unit tests!
///  perfect bounce, perfect sticking, merging - just check for asserts, it shows enough problems

/// \brief Solver computing gravitational interaction of particles.
///
/// The particles are assumed to be point masses. No hydrodynamics or collisions are considered.
class NBodySolver : public ISolver {
private:
    /// Gravity used by the solver
    AutoPtr<IGravity> gravity;

    /// Thread pool for parallelization
    ThreadPool pool;

    AutoPtr<INeighbourFinder> collisionFinder;

    AutoPtr<ICollisionHandler> collisionHandler;

    /// Small value specifying allowed overlap of collided particles, used for numerical stability.
    Float allowedOverlap;

public:
    /// \brief Creates the solver, using the gravity implementation specified by settings.
    explicit NBodySolver(const RunSettings& settings)
        : NBodySolver(settings, makeAuto<BruteForceGravity>()) { // Factory::getGravity(settings)) {
        const Float epsN = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_NORMAL);
        const Float epsT = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_TANGENT);
        allowedOverlap = settings.get<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP);
        collisionHandler = makeAuto<ElasticBounceHandler>(epsN, epsT);
    }

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity))
        , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT))
        , collisionFinder(makeAuto<BruteForceFinder>()) {} // Factory::getFinder(settings)) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
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

    struct CollisionRecord {
        Size index;
        Float t_coll = INFINITY;

        // sort by collision time
        bool operator<(const CollisionRecord& other) const {
            return t_coll < other.t_coll;
        }

        explicit operator bool() const {
            return t_coll < INFINITY;
        }
    };

    /// Checks and resolves particle collisions
    virtual void collide(Storage& storage, Statistics& UNUSED(stats), const Float dt) override {
        ArrayView<Vector> r, v, a;
        tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

        // find the radius of possible colliders
        const Float searchRadius = this->getSearchRadius(r, v, dt);

        // tree for finding collisions
        collisionFinder->build(r);

        // handler determining collison outcomes
        collisionHandler->initialize(storage);

        // create helper storage with simple quantity - remaining time for each particle
        // SharedPtr<Storage> timers = this->getTimerStorage(storage, dt);
        // ArrayView<Float> dts = timers->getValue<Float>(QuantityId::MOVEMENT_TIME);
        Array<Float> dts(r.size());
        dts.fill(dt);

        Array<NeighbourRecord> neighs;

        Array<Size> toRemove;
        for (Size i = 0; i < r.size();) {
            /// \todo search only in front of the particle ?
            collisionFinder->findNeighbours(i, searchRadius, neighs, FinderFlag::FIND_ONLY_SMALLER_H);

            // find the closest collision
            CollisionRecord closestCollision;
            for (NeighbourRecord& n : neighs) {
                const Size j = n.index;

                // temporary hack to force particles into the z=0 plane
                v[i][Z] = 0._f;
                v[j][Z] = 0._f;


                // ASSERT(getLength(r[i] - r[j]) >= 0.8_f * (r[i][H] + r[j][H]), r[i], r[j]);

                /// \todo we need to determine ALL t_colls first, then choose the smallest one. Sorting
                /// neighbours doesn't make a difference!!
                const Float maxDt = min(dts[i], dts[j]);
                Optional<Float> t_coll = this->checkCollision(r[i], v[i], r[j], v[j], maxDt);
                if (t_coll) {
                    ASSERT(t_coll.value() < dts[i] && t_coll.value() < dts[j]);
                    closestCollision = min(closestCollision, CollisionRecord{ j, t_coll.value() });
                }
            }

            if (closestCollision) {
                const Size j = closestCollision.index;
                const Float t_coll = closestCollision.t_coll;

                // advance positions to the point of collision
                r[i] += v[i] * t_coll;
                r[j] += v[j] * t_coll;

                // let collision handler determine the outcome
                toRemove.clear();
                const CollisionResult result = collisionHandler->collide(i, j, toRemove);
                if (result != CollisionResult::NONE) {
                    ASSERT(storage.isValid());
                    // decrease the movement time of collided particles
                    // dts[i] -= t_coll;
                    // dts[j] -= t_coll;
                    ASSERT(dts[i] >= 0._f && dts[j] >= 0._f, dts[i], dts[j]);
                    // apply the removal list (now we can safely invalidate arrayviews)
                    storage.remove(toRemove);
                    // remove it also from all dependent storages, since this is a permanent action
                    storage.propagate([&toRemove](Storage& dependent) { dependent.remove(toRemove); });
                } else {
                    // collision handler rejected the collision, revert the change
                    r[i] -= v[i] * t_coll;
                    r[j] -= v[j] * t_coll;
                }

                switch (result) {
                case CollisionResult::NONE:
                case CollisionResult::BOUNCE:
                    // no need to do anything, case handled just so we are sure we are not forgetting
                    // something
                    break;
                case CollisionResult::EVAPORATION:
                case CollisionResult::MERGER:
                case CollisionResult::FRAGMENTATION:
                    // number of particles changed, we need to reset (possibly invalidated) arrayviews and
                    // rebuild the tree
                    tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);
                    /// \todo dts = timers->getValue<Float>(QuantityId::MOVEMENT_TIME);
                    collisionFinder->build(r);
                    break;
                default:
                    NOT_IMPLEMENTED;
                }

                // process the particle again as there may be more collisions
                continue;
            } else {
                ++i;
            }
        }

        // advance particle positions by the remainder of the movement time
        for (Size i = 0; i < r.size(); ++i) {
            r[i] += v[i] * dts[i];
        }
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {
        // no quantities need to be created, positions and masses are already in the storage
    }

private:
    Float getSearchRadius(ArrayView<const Vector> r, ArrayView<const Vector> v, const Float dt) const {
        // find the largest velocity, so that we know how far to search for potentional impactors
        /// \todo naive implementation, improve
        Float v_max = 0._f;
        Float h_max = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            v_max = max(v_max, getSqrLength(v[i]));
            h_max = max(h_max, r[i][H]);
        }
        v_max = sqrt(v_max);
        // 2x max velocity = max relative velocity
        return 2._f * (v_max * dt + h_max);
    }

    SharedPtr<Storage> getTimerStorage(Storage& storage, const Float dt) const {
        SharedPtr<Storage> timers = makeShared<Storage>();
        Array<Float> dts(storage.getParticleCnt());
        dts.fill(dt);
        timers->insert<Float>(QuantityId::MOVEMENT_TIME, OrderEnum::ZERO, std::move(dts));
        // register the helper storage, so it gets resized together with the parent storage
        storage.addDependent(timers);
        return timers;
    }

    Optional<Float> checkCollision(const Vector& r1,
        const Vector& v1,
        const Vector& r2,
        const Vector& v2,
        const Float dt) {
        const Vector dr = r1 - r2;
        const Vector dv = v1 - v2;
        const Float dvdr = dot(dv, dr);
        if (dvdr >= 0._f) {
            // not moving towards each other, no collision
            return NOTHING;
        }

        const Vector dv_norm = getNormalized(dv);
        //        getLength(dv) < EPS * (getLength(v1) + getLength(v2)) ? getNormalized(dr) :
        //        getNormalized(dv);
        const Vector dr_perp = dr - dot(dv_norm, dr) * dv_norm;
        ASSERT(getSqrLength(dr_perp) < (1._f + EPS) * getSqrLength(dr), dr_perp, dr);
        if (getSqrLength(dr_perp) <= sqr(r1[H] + r2[H])) {
            // on collision trajectory, find the collision time
            const Float dv2 = getSqrLength(dv);
            const Float det = 1._f - (getSqrLength(dr) - sqr(r1[H] + r2[H])) / sqr(dvdr) * dv2;
            ASSERT(det >= 0._f);
            const Float root = (det > 1._f + allowedOverlap) ? 1._f + sqrt(det) : 1._f - sqrt(det);
            const Float t_coll = -dvdr / dv2 * root;
            // collision time can be slightly negative number, corresponding to collision of overlaping
            // particles.
            /// \todo assert that t_coll is not VERY negative (but what very? It's sometimes 4x timestep, hm
            /// ...)
            ASSERT(isReal(t_coll));

            // collision happens if the collision time is less than the current timestep
            if (t_coll < dt) {
                return t_coll;
            }
        }
        return NOTHING;
    }
};

NAMESPACE_SPH_END
