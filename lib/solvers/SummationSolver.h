#pragma once

/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/GenericSolver.h"
#include "thread/AtomicFloat.h"

NAMESPACE_SPH_BEGIN

/// Uses density and specific energy as independent variables. Density is solved by direct summation, using
/// self-consistent solution with smoothing length. Energy is evolved using energy equation.
class SummationSolver : public GenericSolver {
private:
    Float eta;
    Size maxIteration;
    Float targetDensityDifference;
    Array<Float> rho, h;

    LutKernel<DIMENSIONS> densityKernel;

public:
    SummationSolver(const RunSettings& settings)
        : GenericSolver(settings, getEquations(settings)) {
        eta = settings.get<Float>(RunSettingsId::SPH_KERNEL_ETA);
        maxIteration = settings.get<int>(RunSettingsId::SUMMATION_MAX_ITERATIONS);
        targetDensityDifference = settings.get<Float>(RunSettingsId::SUMMATION_DENSITY_DELTA);
        densityKernel = Factory::getKernel<DIMENSIONS>(settings);
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
        storage.insert<Float>(
            QuantityId::DENSITY, OrderEnum::ZERO, material.getParam<Float>(BodySettingsId::DENSITY));
        material.minimal(QuantityId::DENSITY) = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
        material.range(QuantityId::DENSITY) = material.getParam<Range>(BodySettingsId::DENSITY_RANGE);
        equations.create(storage, material);
    }

private:
    EquationHolder getEquations(const RunSettings& settings) {
        EquationHolder equations;
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT)) {
            equations += makeTerm<PressureForce>();
        }
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
            equations += makeTerm<SolidStressForce>(settings);
        }
        equations += makeTerm<StandardAV>();

        // we evolve density and smoothing length ourselves (outside the equation framework)

        return equations;
    }

    virtual void beforeLoop(Storage& storage, Statistics& stats) override {
        GenericSolver::beforeLoop(storage, stats);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);

        rho.resize(r.size());
        rho.fill(EPS);
        h.resize(r.size());
        Atomic<Float> totalDiff = 0._f; // we only sum few numbers, so it does not have to be thread local.
        auto functor = [this, r, m, &totalDiff](const Size n1, const Size n2, ThreadData& data) {
            Float diff = 0._f;
            for (Size i = n1; i < n2; ++i) {
                /// \todo do we have to recompute neighbours in every iteration?
                // find all neighbours
                finder->findNeighbours(i, h[i] * kernel.radius(), data.neighs, EMPTY_FLAGS);
                // find density and smoothing length by self-consistent solution.
                const Float rho0 = rho[i]; /// \todo first step has undefined rho
                rho[i] = 0._f;
                for (auto& n : data.neighs) {
                    const int j = n.index;
                    /// \todo can this be generally different kernel than the one used for derivatives?
                    rho[i] += m[j] * densityKernel.value(r[i] - r[j], h[i]);
                }
                h[i] = eta * root<DIMENSIONS>(m[i] / rho[i]);
                diff += abs(rho[i] - rho0) / rho0;
            }
            totalDiff += diff;
        };

        finder->build(r);
        Size iterationIdx = 0;
        for (; iterationIdx < maxIteration; ++iterationIdx) {
            parallelFor(pool, threadData, 0, r.size(), granularity, functor);
            if (totalDiff / r.size() < targetDensityDifference) {
                break;
            }
        }
        stats.set(StatisticsId::SOLVER_SUMMATION_ITERATIONS, int(iterationIdx));
        // save computed values
        std::swap(storage.getValue<Float>(QuantityId::DENSITY), rho);
        for (Size i = 0; i < r.size(); ++i) {
            r[i][H] = h[i];
        }
    }
};

NAMESPACE_SPH_END
