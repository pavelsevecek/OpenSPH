#pragma once

/// \file SummationSolver.h
/// \brief Solver using direction summation to compute density and smoothing length.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/solvers/GenericSolver.h"
#include "thread/AtomicFloat.h"

NAMESPACE_SPH_BEGIN

/// Uses density and specific energy as independent variables. Density is solved by direct summation, using
/// self-consistent solution with smoothing length. Energy is evolved using energy equation.
class SummationSolver : public GenericSolver {
private:
    Float eta;
    Size maxIteration;
    Float targetDensityDifference;
    bool adaptiveH;
    Array<Float> rho, h;

    LutKernel<DIMENSIONS> densityKernel;

public:
    SummationSolver(const RunSettings& settings, const EquationHolder& additionalEquations = {})
        : GenericSolver(settings, getEquations(settings) + additionalEquations) {
        eta = settings.get<Float>(RunSettingsId::SPH_KERNEL_ETA);
        targetDensityDifference = settings.get<Float>(RunSettingsId::SUMMATION_DENSITY_DELTA);
        densityKernel = Factory::getKernel<DIMENSIONS>(settings);
        Flags<SmoothingLengthEnum> flags = Flags<SmoothingLengthEnum>::fromValue(
            settings.get<int>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH));
        adaptiveH = !flags.has(SmoothingLengthEnum::CONST);
        maxIteration = adaptiveH ? settings.get<int>(RunSettingsId::SUMMATION_MAX_ITERATIONS) : 1;
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
        material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
        equations.create(storage, material);
    }

private:
    EquationHolder getEquations(const RunSettings& settings) {
        EquationHolder equations;
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT)) {
            equations += makeTerm<PressureForce>(settings);
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
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);

        // initialize density estimate
        rho.resize(r.size());
        rho.fill(EPS);

        // initialize smoothing length to current values
        h.resize(r.size());
        for (Size i = 0; i < r.size(); ++i) {
            h[i] = r[i][H];
        }

        Atomic<Float> totalDiff = 0._f; // we only sum few numbers, so it does not have to be thread local.
        auto functor = [this, r, m, &totalDiff](const Size n1, const Size n2, ThreadData& data) {
            Float diff = 0._f;
            for (Size i = n1; i < n2; ++i) {
                /// \todo do we have to recompute neighbours in every iteration?
                // find all neighbours
                finder->findNeighbours(i, h[i] * kernel.radius(), data.neighs, EMPTY_FLAGS);
                ASSERT(data.neighs.size() > 0, data.neighs.size());
                // find density and smoothing length by self-consistent solution.
                const Float rho0 = rho[i];
                rho[i] = 0._f;
                for (auto& n : data.neighs) {
                    const Size j = n.index;
                    /// \todo can this be generally different kernel than the one used for derivatives?
                    rho[i] += m[j] * densityKernel.value(r[i] - r[j], h[i]);
                }
                ASSERT(rho[i] > 0._f, rho[i]);
                h[i] = eta * root<DIMENSIONS>(m[i] / rho[i]);
                ASSERT(h[i] > 0._f);
                diff += abs(rho[i] - rho0) / (rho[i] + rho0);
            }
            totalDiff += diff;
        };

        finder->build(r);
        Size iterationIdx = 0;
        for (; iterationIdx < maxIteration; ++iterationIdx) {
            parallelFor(*pool, threadData, 0, r.size(), granularity, functor);
            const Float diff = totalDiff / r.size();
            if (diff < targetDensityDifference) {
                break;
            }
        }
        stats.set(StatisticsId::SOLVER_SUMMATION_ITERATIONS, int(iterationIdx));
        // save computed values
        std::swap(storage.getValue<Float>(QuantityId::DENSITY), rho);
        if (adaptiveH) {
            for (Size i = 0; i < r.size(); ++i) {
                r[i][H] = h[i];
            }
        }
    }

    virtual void sanityCheck() const override {
        // we handle smoothing lengths ourselves, bypass the check of equations
    }
};

NAMESPACE_SPH_END
