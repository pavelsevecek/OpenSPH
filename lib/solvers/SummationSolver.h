#pragma once

/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

/// Uses density and specific energy as independent variables. Density is solved by direct summation, using
/// self-consistent solution with smoothing length. Energy is evolved using energy equation.
class SummationSolver : public GenericSolver {
private:
    Float eta;
	Size maxIterations;
	Float targetDensityDifference;
	Array<Float> rho, h;

public:
    SummationSolver(const RunSettings& settings)
        : GenericSolver(settings, getEquations()) {
        eta = settings.get<Float>(RunSettingsId::SPH_KERNEL_ETA);
		maxIteration = settings.get<int>(RunSettingsId::SOLVER_SUMMATION_MAX_ITERATION);
		targetDensityDifference = settings.get<Float>(RunSettingsId::SOLVER_SUMMATION_DENSITY_DIFFERENCE);
    }
	
    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
		storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, material.getParam<Float>(BodySettingsId::DENSITY);
		material.minimal(QuantityId::DENSITY) = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
		material.range(QuantityId::DENSITY) = material.getParam<Range>(BodySettingsId::DENSITY_RANGE);
        equations.create(storage, material);
    }
	
private:
    EquationHolder getEquations() {
		 EquationHolder equations;
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_GRAD_P)) {
            equations += makeTerm<PressureForce>();
        }
        if (settings.get<bool>(RunSettingsId::MODEL_FORCE_DIV_S)) {
            equations += makeTerm<StressForce>(settings);
        }
        equations += makeTerm<StandardAV>(settings);

        // we evolve density and smoothing length ourselves (outside the equation framework)
		
        return equations;
	}
	
	virtual void beforeLoop(Storage& storage, Statistics& stats) override {
		ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
		AtomicFloat totalDiff; // we only sum few numbers, so it does not have to be thread local.
		auto functor = [this, iterationIdx](const Size n1, const Size n2, ThreadData& data) {
			Float diff = 0._f;
			for (Size i = n1; i < n2; ++i) {
				/// \todo do we have to recompute neighbours in every iteration?
				// find all neighbours
                finder->findNeighbours(i, h[i] * kernel.radius(), data.neighs, EMPTY_FLAGS);
                // find density and smoothing length by self-consistent solution.
				const Float rho0 = rho[i]; /// \todo first step has undefined rho
                rho[i] = 0._f;
                for (auto& n : neighs) {
                    const int j = n.index;
                    rho[i] += m[j] * kernel.value(r[i] - r[j], h[i]);
                }
                h[i] = eta * root<DIMENSIONS>(m[i] / rho[i]);
				diff += abs(rho[i] - rho0);
            }	
            ASSERT(n1 != n2); // this should be guaranteed by parallelFor
			diff /= (n2 - n1);
		};
		Size iterationIdx = 0;
		for (; iterationIdx < maxIteration; ++iterationIdx) {
             parallelFor(pool, threadData, 0, r.size(), granularity, functor);
			 // } && abs(previousRho - accumulatedRho[i]) / previousRho > 1.e-3_f) {
				 // break;
		}
		stats.set(StatisticsId::SOLVER_SUMMATION_ITERATIONS, iterationIdx);
	} 
};

NAMESPACE_SPH_END
