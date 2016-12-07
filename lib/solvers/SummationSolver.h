#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "solvers/AbstractSolver.h"
#include "solvers/Accumulator.h"
#include "sph/av/Standard.h"
#include "sph/boundary/Boundary.h"
#include "sph/kernel/Kernel.h"
#include "storage/QuantityKey.h"
#include "system/Profiler.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// Uses density and specific energy as independent variables. Density is solved by direct summation, using
/// self-consistent solution with smoothing length. Energy is evolved using energy equation.
template <int d, typename TForce>
class SummationSolver : public SolverBase<d, TForce> {
private:
    Array<Float> accumulatedRho;
    Array<Float> accumulatedH;
    Float eta;

    TForce force;



public:
    SummationSolver(const Settings<GlobalSettingsIds>& settings);

    ~SummationSolver(); // necessary because of unique_ptrs to incomplete types

    virtual void compute(Storage& storage) override {
        const int size = storage.getParticleCnt();

        ArrayView<Vector> r, v, dv;
        ArrayView<Float> rho, drho, u, du, p, m, cs;
        PROFILE_SCOPE("SummationSolver::compute (getters)");

        // fetch quantities from storage
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        tieToArray(rho, drho) = storage.getAll<Float>(QuantityKey::RHO);
        tieToArray(u, du)     = storage.getAll<Float>(QuantityKey::U);
        // tie(p, m, cs) = storage.get<QuantityKey::P, QuantityKey::M, QuantityKey::CS>();
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
        force.update(storage);

        PROFILE_NEXT("SummationSolver::compute (init)");
        accumulatedRho.resize(r.size());
        accumulatedRho.fill(0._f);
        accumulatedH.resize(r.size());
        accumulatedH.fill(0._f);

        // clamp smoothing length
        /// \todo generalize clamping, min / max values
        for (Float& h : componentAdapter(r, H)) {
            h = Math::max(h, 1.e-12_f);
        }

        // find new pressure
        this->computeMaterial(storage);

        this->finder->build(r);

        // we symmetrize kernel by averaging kernel values
        SymW<d> w(this->kernel);

        PROFILE_NEXT("SummationSolver::compute (main cycle)");
        int maxIteration = 0;
        for (int i = 0; i < size; ++i) {
            SCOPE_STOP;
            // find neighbours
            const Float pRhoInvSqr = p[i] / Math::sqr(rho[i]);
            ASSERT(Math::isReal(pRhoInvSqr));
            accumulatedH[i]   = r[i][H];
            Float previousRho = EPS;
            accumulatedRho[i] = rho[i];
            int iterationIdx  = 0;
            while (iterationIdx < 20 && Math::abs(previousRho - accumulatedRho[i]) / previousRho > 1.e-3_f) {
                previousRho = accumulatedRho[i];
                if (iterationIdx == 0 || accumulatedH[i] > r[i][H]) {
                    // smoothing length increased, we need to recompute neighbours
                    this->finder->findNeighbours(i, accumulatedH[i] * this->kernel.radius(), this->neighs);
                }
                SCOPE_RESUME;
                // find density and smoothing length by self-consistent solution.
                accumulatedRho[i] = 0._f;
                for (const auto& neigh : this->neighs) {
                    const int j = neigh.index;
                    accumulatedRho[i] += m[j] * this->kernel.value(r[i] - r[j], accumulatedH[i]);
                }
                accumulatedH[i] = eta * Math::root<d>(m[i] / accumulatedRho[i]);
                iterationIdx++;
            }
            maxIteration = Math::max(maxIteration, iterationIdx);
            for (const auto& neigh : this->neighs) {
                const int j = neigh.index;
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

                // compute forces (accelerations) + artificial viscosity
                dv[i] += m[j] * force.dv(i, j, grad);
                du[i] += m[j] * force.du(i, j, grad);
            }
        }
        std::cout << "Max " << maxIteration << " iterations " << std::endl;

        PROFILE_NEXT("SummationSolver::compute (solvers)")

        for (int i = 0; i < size; ++i) {
            rho[i]   = accumulatedRho[i];
            r[i][H]  = accumulatedH[i];
            v[i][H]  = 0._f;
            dv[i][H] = 0._f;
        }

        // Apply boundary conditions
        if (this->boundary) {
            this->boundary->apply(storage);
        }
    }
};

NAMESPACE_SPH_END
