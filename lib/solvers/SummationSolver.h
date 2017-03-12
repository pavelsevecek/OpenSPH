#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "quantities/QuantityIds.h"
#include "solvers/AbstractSolver.h"
#include "solvers/Accumulator.h"
#include "sph/av/Standard.h"
#include "sph/boundary/Boundary.h"
#include "sph/forces/StressForce.h"
#include "sph/kernel/Kernel.h"
#include "system/Profiler.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// Uses density and specific energy as independent variables. Density is solved by direct summation, using
/// self-consistent solution with smoothing length. Energy is evolved using energy equation.
template <typename Force, int D>
class SummationSolver : public SolverBase<D>, Module<Force> {
private:
    Array<Float> accumulatedRho;
    Array<Float> accumulatedH;
    Float eta;

    Force force;

    static constexpr int dim = D;

public:
    SummationSolver(const GlobalSettings& settings)
        : SolverBase<D>(settings)
        , Module<Force>(force)
        , force(settings) {
        eta = settings.get<Float>(GlobalSettingsIds::SPH_KERNEL_ETA);
    }

    virtual void integrate(Storage& storage, Statistics& UNUSED(stats)) override {
        const int size = storage.getParticleCnt();

        ArrayView<Vector> r, v, dv;
        ArrayView<Float> rho, m;
        PROFILE_SCOPE("SummationSolver::compute (getters)");

        // fetch quantities from storage
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        tie(rho, m) = storage.getValues<Float>(QuantityIds::DENSITY, QuantityIds::MASSES);
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
        this->updateModules(storage);

        PROFILE_NEXT("SummationSolver::compute (init)");
        accumulatedRho.resize(r.size());
        accumulatedRho.fill(0._f);
        accumulatedH.resize(r.size());
        accumulatedH.fill(0._f);

        // clamp smoothing length
        /// \todo generalize clamping, min / max values
        for (Float& h : componentAdapter(r, H)) {
            h = max(h, 1.e-12_f);
        }

        this->finder->build(r);

        // we symmetrize kernel by averaging kernel values
        SymW<dim> w(this->kernel);

        PROFILE_NEXT("SummationSolver::compute (main cycle)");
        int maxIteration = 0;
        for (int i = 0; i < size; ++i) {
            // find neighbours
            accumulatedH[i] = r[i][H];
            Float previousRho = EPS;
            accumulatedRho[i] = rho[i];
            int iterationIdx = 0;
            while (iterationIdx < 20 && abs(previousRho - accumulatedRho[i]) / previousRho > 1.e-3_f) {
                previousRho = accumulatedRho[i];
                if (iterationIdx == 0 || accumulatedH[i] > r[i][H]) {
                    // smoothing length increased, we need to recompute neighbours
                    this->finder->findNeighbours(i, accumulatedH[i] * this->kernel.radius(), this->neighs);
                }
                // find density and smoothing length by self-consistent solution.
                accumulatedRho[i] = 0._f;
                for (const auto& neigh : this->neighs) {
                    const int j = neigh.index;
                    accumulatedRho[i] += m[j] * this->kernel.value(r[i] - r[j], accumulatedH[i]);
                }
                accumulatedH[i] = eta * root<dim>(m[i] / accumulatedRho[i]);
                iterationIdx++;
            }
            maxIteration = max(maxIteration, iterationIdx);
            for (const auto& neigh : this->neighs) {
                const int j = neigh.index;
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

                // compute forces (accelerations) + artificial viscosity
                this->accumulateModules(i, j, grad);
            }
        }

        PROFILE_NEXT("SummationSolver::compute (solvers)")

        this->integrateModules(storage);
        for (int i = 0; i < size; ++i) {
            rho[i] = accumulatedRho[i];
            r[i][H] = accumulatedH[i];
            v[i][H] = 0._f;
            dv[i][H] = 0._f;
        }

        // Apply boundary conditions
        if (this->boundary) {
            this->boundary->apply(storage);
        }
    }

    virtual void initialize(Storage& storage, const BodySettings& settings) const override {
        storage.insert<Float, OrderEnum::ZERO>(QuantityIds::DENSITY,
            settings.get<Float>(BodySettingsIds::DENSITY),
            settings.get<Range>(BodySettingsIds::DENSITY_RANGE));
        storage.insert<Float, OrderEnum::FIRST>(QuantityIds::ENERGY,
            settings.get<Float>(BodySettingsIds::ENERGY),
            settings.get<Range>(BodySettingsIds::ENERGY_RANGE));
        this->initializeModules(storage, settings);
    }
};

NAMESPACE_SPH_END
