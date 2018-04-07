#include "sph/solvers/SummationSolver.h"
#include "objects/finders/NeighbourFinder.h"
#include "sph/equations/av/Standard.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Statistics.h"
#include "thread/AtomicFloat.h"

NAMESPACE_SPH_BEGIN

static EquationHolder getEquations(const RunSettings& settings) {
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
    EquationHolder equations;
    if (forces.has(ForceEnum::PRESSURE_GRADIENT)) {
        equations += makeTerm<PressureForce>();
    }
    if (forces.has(ForceEnum::SOLID_STRESS)) {
        equations += makeTerm<SolidStressForce>(settings);
    }
    equations += makeTerm<StandardAV>();

    // we evolve density and smoothing length ourselves (outside the equation framework)

    ASSERT(!forces.has(ForceEnum::GRAVITY), "Summation solver cannot be currently used with gravity");

    return equations;
}

SummationSolver::SummationSolver(const RunSettings& settings, const EquationHolder& additionalEquations)
    : SymmetricSolver(settings, getEquations(settings) + additionalEquations) {
    eta = settings.get<Float>(RunSettingsId::SPH_KERNEL_ETA);
    targetDensityDifference = settings.get<Float>(RunSettingsId::SUMMATION_DENSITY_DELTA);
    densityKernel = Factory::getKernel<DIMENSIONS>(settings);
    Flags<SmoothingLengthEnum> flags =
        Flags<SmoothingLengthEnum>::fromValue(settings.get<int>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH));
    adaptiveH = !flags.has(SmoothingLengthEnum::CONST);
    maxIteration = adaptiveH ? settings.get<int>(RunSettingsId::SUMMATION_MAX_ITERATIONS) : 1;
}

void SummationSolver::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
    material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);
    storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
    equations.create(storage, material);
}

void SummationSolver::beforeLoop(Storage& storage, Statistics& stats) {
    SymmetricSolver::beforeLoop(storage, stats);
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

    Atomic<Float> totalDiff = 0._f; /// \todo use thread local for summing?
    auto functor = [this, r, m, &totalDiff](const Size i, ThreadData& data) {
        /// \todo do we have to recompute neighbours in every iteration?
        // find all neighbours
        finder->findAll(i, h[i] * kernel.radius(), data.neighs);
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
        totalDiff += abs(rho[i] - rho0) / (rho[i] + rho0);
    };

    finder->build(r);
    Size iterationIdx = 0;
    for (; iterationIdx < maxIteration; ++iterationIdx) {
        parallelFor(pool, threadData, 0, r.size(), granularity, functor);
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

void SummationSolver::sanityCheck(const Storage& UNUSED(storage)) const {
    // we handle smoothing lengths ourselves, bypass the check of equations
}

NAMESPACE_SPH_END
