#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/IGravity.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "thread/Pool.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

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

public:
    /// \brief Creates the solver, using the gravity implementation specified by settings.
    explicit NBodySolver(const RunSettings& settings)
        : NBodySolver(settings, Factory::getGravity(settings)) {}

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity))
        , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT))
        , collisionFinder(Factory::getFinder(settings)) {}

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
    virtual void collide(Storage& storage, Statistics& UNUSED(stats), const Float dt) override {
        // const Float restitution = 1._f; // coeff of restitution; 0 -> perfect sticking, 1 -> perfect bounce

        ArrayView<Vector> r, v, a;
        tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITION);

        // find the largest velocity, so that we know how far to search for potentional impactors
        /// \todo naive implementation, improve
        Float v_max = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            v_max = max(v_max, getSqrLength(v[i]));
        }
        v_max = sqrt(v_max);

        collisionFinder->build(r);
        Array<Pair<Size>> toMerge;

        Array<NeighbourRecord> neighs;
        for (Size i = 0; i < r.size(); ++i) {
            // find each pair only once
            collisionFinder->findNeighbours(
                i, 2._f * (v_max * dt + r[i][H]), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
            for (NeighbourRecord& n : neighs) {
                const Size j = n.index;
                const Vector dr = r[i] - r[j];
                const Vector dv = v[i] - v[j];
                const Float dvdr = dot(dv, dr);
                if (dvdr < 0._f) {
                    // moving towards each other, possible collision
                    const Vector dv_norm = getNormalized(dv);
                    const Vector dr_perp = dr - dot(dv_norm, dr) * dv_norm;
                    ASSERT(getSqrLength(dr_perp) < getSqrLength(dr), dr_perp, dr);
                    if (getSqrLength(dr_perp) <= sqr(r[i][H] + r[j][H])) {
                        // on collision trajectory, find the collision time
                        const Float dv2 = getSqrLength(dv);
                        const Float det =
                            1._f - (getSqrLength(dr) - sqr(r[i][H] + r[j][H])) / sqr(dvdr) * dv2;
                        ASSERT(det >= 0);
                        const Float root = (det > 1._f) ? 1._f + sqrt(det) : 1._f - sqrt(det);
                        const Float t_coll = -dvdr / dv2 * root;

                        if (t_coll < dt) {
                            // collision happens in this timestep, find the new positions of spheres
                            const Vector r1 = r[i] + v[i] * t_coll;
                            const Vector r2 = r[j] + v[j] * t_coll;
                            /*ASSERT(almostEqual(getSqrLength(r1 - r2), r[i][H] + r[j][H]),
                                getSqrLength(r1 - r2),
                                r[i][H] + r[j][H]);*/

                            // keep only the perpendicular component of the acceleration
                            const Vector dir = getNormalized(r1 - r2);
                            a[i] -= dot(a[i], dir) * dir; // check signs!
                            a[j] += dot(a[j], dir) * dir;

                            toMerge.push(Pair<Size>{ i, j });
                        }
                    }
                }
                ASSERT(r[i][H] + r[j][H] <= getLength(dr), r[i][H] + r[j][H], getLength(dr));
            }
            // no collision, just advance positions
            r[i] += v[i] * dt;
        }

        for (auto& mergers : toMerge) {
            storage.remove(mergers, true);
        }
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {
        // no quantities need to be created, positions and masses are already in the storage
    }

private:
};

NAMESPACE_SPH_END
