#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/IGravity.h"
#include "system/Settings.h"
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
    explicit NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity))
        , pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT)) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        gravity->build(storage);

        ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITIONS);
        gravity->evalAll(pool, dv, stats);
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        // no quantities need to be created, positions and masses are already in the storage
    }

private:
    /// Checks and resolves particle collisions
    void collide(Storage& storage, const Float dt) {
        const Float restitution = 1._f; // coeff of restitution; 0 -> perfect sticking, 1 -> perfect bounce

        ArrayView<const Vector> r, v, a;
        tie(r, v, a) = storage.getAll<Vector>(QuantityId::POSITIONS);

        Array<NeighbourRecord> neighs;
        for (Size i = 0; i < r.size(); ++i) {
            collisionFinder->findNeighbours(i, v_max * dt, neighs);
            for (NeighbourRecord& n : neighs) {
                const Size j = n.index;
                const Vector dr = r[i] - r[j];
                const Vector dv = v[i] - v[j];
                const Float dvdr = dot(dv, dr);
                if (dvdr < 0._f) {
                    // moving towards each other, possible collision
                    const Vector dv_norm = getNormalized(dv);
                    const Vector dr_perp = dr - dot(dv_norm, dr) * dv_norm;
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
                            ASSERT(almostEqual(getSqrLength(r1 - r2), r[i][H] + r[j][H]));

                            // keep only the perpendicular component of the acceleration
                            const Vector dir = getNormalized(r1 - r2);
                            dv[i] -= dot(dv[i], dir) * dir; // check signs!
                            dv[j] += dot(dv[j], dir) * dir;
                        }
                    }
                }
            }
        }
    }
};

NAMESPACE_SPH_END
