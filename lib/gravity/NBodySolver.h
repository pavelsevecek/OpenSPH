#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "gravity/IGravity.h"
#include "io/Logger.h"
#include "objects/finders/BruteForceFinder.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"
#include <set>

NAMESPACE_SPH_BEGIN


struct CollisionRecord {
    Size i;
    Size j;
    Float t_coll = INFINITY;

    // sort by collision time
    bool operator<(const CollisionRecord& other) const {
        return t_coll < other.t_coll;
    }

    /*bool operator==(const CollisionRecord& other) const {
        if (!almostEqual(t_coll, other.t_coll, 1.e-6_f)) {
            return false;
        }
        return (i == other.i && j == other.j) || (i == other.j && j == other.i);
    }

    bool operator!=(const CollisionRecord& other) const {
        return !(*this == other);
    }*/

    explicit operator bool() const {
        return t_coll < INFINITY;
    }
};

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

    /*struct ThreadData {
        Array<NeighbourRecord> neighs;
    };*/

    // for single-threaded search (should be parallelized in the future)
    Array<NeighbourRecord> neighs;

    // ThreadLocal<ThreadData> threadData;

    std::mutex mutex;

public:
    /// \brief Creates the solver, using the gravity implementation specified by settings.
    explicit NBodySolver(const RunSettings& settings)
        : NBodySolver(settings, makeAuto<BruteForceGravity>()) { // Factory::getGravity(settings)) {
        const Float epsN = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_NORMAL);
        const Float epsT = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_TANGENT);
        /// \todo check if this parameter is still needed
        allowedOverlap = settings.get<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP);
        collisionHandler = makeAuto<ElasticBounceHandler>(epsN, epsT);
    }

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity))
        , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT))
        , collisionFinder(Factory::getFinder(settings)) {}
    //, threadData(pool) {}

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


    /// Checks and resolves particle collisions
    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override {
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
        /*timers.resize(r.size());
        timers.fill(0._f);*/


        Size collisionCounter = 0;

        Array<Size> toRemove;
        std::set<CollisionRecord> collisions;
        std::set<Size> removedIdx;

        // first pass - find all collisions and sort them by collision time
        for (Size i = 0; i < r.size(); ++i) {
            if (CollisionRecord col = this->findClosestCollision(i, searchRadius, dt, neighs, r, v)) {
                collisions.insert(col);
            }
        }

        Float time = 0._f; // dt;
        while (!collisions.empty()) {
            const CollisionRecord& col = *collisions.begin();
            const Size i = col.i;
            const Size j = col.j;
            const Float t_coll = col.t_coll;
            ASSERT(t_coll < dt);

            // advance all positions to the collision time
            if (t_coll != time) {
                for (Size i = 0; i < r.size(); ++i) {
                    r[i] += v[i] * (t_coll - time);
                }
            }

            // handle the collision
            toRemove.clear();
            const CollisionResult result = collisionHandler->collide(i, j, toRemove);
            if (result != CollisionResult::NONE) {
                ASSERT(storage.isValid());
                time = t_coll; // move the time to the current collision time

                collisionCounter++;

                // apply the removal list (now we can safely invalidate arrayviews)
                // storage.remove(toRemove);
                // remove it also from all dependent storages, since this is a permanent action
                // storage.propagate([&toRemove](Storage& dependent) { dependent.remove(toRemove); });

                // remove all collisions containing either i or j
                removedIdx.clear();
                for (auto iter = collisions.begin(); iter != collisions.end();) {
                    const CollisionRecord& c = *iter;
                    if (c.i == i || c.i == j || c.j == i || c.j == j) {
                        removedIdx.insert(c.i);
                        removedIdx.insert(c.j);
                        iter = collisions.erase(iter);
                    } else {
                        ++iter;
                    }
                }

                // find new collisions for removed indices
                for (Size idx : removedIdx) {
                    if (CollisionRecord c =
                            this->findClosestCollision(idx, searchRadius, dt - time, neighs, r, v)) {
                        if ((c.i == i && c.j == j) || (c.j == i && c.i == j)) {
                            // don't process the same pair twice in a row
                            continue;
                        }
                        c.t_coll += time;
                        collisions.insert(c);
                    }
                }
            }
        }
        // advance all positions by the remaining time
        for (Size i = 0; i < r.size(); ++i) {
            r[i] += v[i] * (dt - time);
        }
        stats.set(StatisticsId::COLLISION_COUNT, int(collisionCounter));
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
        return v_max * dt + h_max;
    }

    CollisionRecord findClosestCollision(const Size i,
        const Float globalRadius,
        const Float dt,
        Array<NeighbourRecord>& neighs,
        ArrayView<Vector> r,
        ArrayView<Vector> v) const {
        // maximum travel of i-th particle
        const Float localRadius = r[i][H] + getLength(v[i]) * dt;
        // since we don't know a priori the travel distance of neighbours, use the maximum of all
        collisionFinder->findNeighbours(i, globalRadius + localRadius, neighs, EMPTY_FLAGS);

        CollisionRecord closestCollision;
        for (NeighbourRecord& n : neighs) {
            const Size j = n.index;
            if (j == i) {
                continue;
            }
            ASSERT(getLength(r[i] - r[j]) >= 0.9_f * (r[i][H] + r[j][H]), r[i], r[j]);

            Optional<Float> t_coll = this->checkCollision(r[i], v[i], r[j], v[j], dt);
            if (t_coll) {
                ASSERT(t_coll.value() < dt && t_coll.value() < dt);
                closestCollision = min(closestCollision, CollisionRecord{ i, j, t_coll.value() });
            }
        }
        return closestCollision;
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
        const Float dt) const {
        const Vector dr = r1 - r2;
        const Vector dv = v1 - v2;
        const Float dvdr = dot(dv, dr);
        if (dvdr > 0._f) {
            // not moving towards each other, no collision
            return NOTHING;
        }

        //        getLength(dv) < EPS * (getLength(v1) + getLength(v2)) ? getNormalized(dr) :
        //        getNormalized(dv);
        const Vector dr_perp = dr - dot(dv, dr) * dv / getSqrLength(dv);
        ASSERT(getSqrLength(dr_perp) < (1._f + EPS) * getSqrLength(dr), dr_perp, dr);
        if (getSqrLength(dr_perp) <= sqr(r1[H] + r2[H])) {
            // on collision trajectory, find the collision time
            const Float dv2 = getSqrLength(dv);
            const Float det = 1._f - (getSqrLength(dr) - sqr(r1[H] + r2[H])) / sqr(dvdr) * dv2;
            // ASSERT(det >= 0._f);
            const Float root =
                (det > 1._f /*+ allowedOverlap*/) ? 1._f + sqrt(det) : 1._f - sqrt(max(0._f, det));
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
