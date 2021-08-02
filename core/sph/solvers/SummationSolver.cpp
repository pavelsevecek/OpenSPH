#include "sph/solvers/SummationSolver.h"
#include "objects/finders/NeighborFinder.h"
#include "quantities/IMaterial.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/av/Standard.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "thread/AtomicFloat.h"

NAMESPACE_SPH_BEGIN

static EquationHolder getEquations(const RunSettings& settings) {
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
    EquationHolder equations;
    if (forces.has(ForceEnum::PRESSURE)) {
        equations += makeTerm<PressureForce>();
    }
    if (forces.has(ForceEnum::SOLID_STRESS)) {
        equations += makeTerm<SolidStressForce>(settings);
    }
    if (auto av = Factory::getArtificialViscosity(settings)) {
        equations += EquationHolder(std::move(av));
    }

    // we evolve density and smoothing length ourselves (outside the equation framework),
    // so make sure it does not change outside the solver
    equations += makeTerm<ConstSmoothingLength>();

    SPH_ASSERT(
        !forces.has(ForceEnum::SELF_GRAVITY), "Summation solver cannot be currently used with gravity");

    return equations;
}

template <Size Dim>
SummationSolver<Dim>::SummationSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& additionalEquations,
    AutoPtr<IBoundaryCondition>&& bc)
    : SymmetricSolver<Dim>(scheduler, settings, getEquations(settings) + additionalEquations, std::move(bc)) {

    targetDensityDifference = settings.get<Float>(RunSettingsId::SPH_SUMMATION_DENSITY_DELTA);
    densityKernel = Factory::getKernel<Dim>(settings);
    Flags<SmoothingLengthEnum> flags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH);
    adaptiveH = (flags != EMPTY_FLAGS);
    maxIteration = adaptiveH ? settings.get<int>(RunSettingsId::SPH_SUMMATION_MAX_ITERATIONS) : 1;
}

template <Size Dim>
SummationSolver<Dim>::SummationSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& additionalEquations)
    : SummationSolver(scheduler, settings, additionalEquations, Factory::getBoundaryConditions(settings)) {}

template <Size Dim>
void SummationSolver<Dim>::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
    material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);
    storage.insert<Size>(QuantityId::NEIGHBOR_CNT, OrderEnum::ZERO, 0);
    this->equations.create(storage, material);
}

template <Size Dim>
void SummationSolver<Dim>::beforeLoop(Storage& storage, Statistics& stats) {
    SymmetricSolver<Dim>::beforeLoop(storage, stats);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASS);

    // initialize density estimate
    rho.resize(r.size());
    rho.fill(EPS);

    // initialize smoothing length to current values
    h.resize(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        h[i] = r[i][H];
        SPH_ASSERT(h[i] > 0._f);
    }

    Float eta = 0._f;
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        // all eta's should be the same, but let's use max to be sure
        eta = max(eta, storage.getMaterial(matId)->getParam<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA));
    }

    Atomic<Float> totalDiff = 0._f; /// \todo use thread local for summing?
    auto functor = [this, r, m, eta, &totalDiff](const Size i, ThreadData& data) {
        /// \todo do we have to recompute neighbors in every iteration?
        // find all neighbors
        this->finder->findAll(i, h[i] * densityKernel.radius(), data.neighs);
        SPH_ASSERT(data.neighs.size() > 0, data.neighs.size());
        // find density and smoothing length by self-consistent solution.
        const Float rho0 = rho[i];
        rho[i] = 0._f;
        for (auto& n : data.neighs) {
            const Size j = n.index;
            /// \todo can this be generally different kernel than the one used for derivatives?
            rho[i] += m[j] * densityKernel.value(r[i] - r[j], h[i]);
        }
        SPH_ASSERT(rho[i] > 0._f, rho[i]);
        h[i] = eta * root<Dim>(m[i] / rho[i]);
        SPH_ASSERT(h[i] > 0._f);
        totalDiff += abs(rho[i] - rho0) / (rho[i] + rho0);
    };

    this->finder->build(this->scheduler, r);
    Size iterationIdx = 0;
    for (; iterationIdx < maxIteration; ++iterationIdx) {
        parallelFor(this->scheduler, this->threadData, 0, r.size(), functor);
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
            SPH_ASSERT(r[i][H] > 0._f);
        }
    }
}

template <Size Dim>
void SummationSolver<Dim>::sanityCheck(const Storage& UNUSED(storage)) const {
    // we handle smoothing lengths ourselves, bypass the check of equations
}

template class SummationSolver<1>;
template class SummationSolver<2>;
template class SummationSolver<3>;

NAMESPACE_SPH_END
