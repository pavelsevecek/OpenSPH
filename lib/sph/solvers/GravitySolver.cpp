#include "sph/solvers/GravitySolver.h"
#include "gravity/SphericalGravity.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/EnergyConservingSolver.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Factory.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

template <typename TSphSolver>
GravitySolver<TSphSolver>::GravitySolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& equations)
    : GravitySolver(scheduler, settings, equations, Factory::getGravity(settings)) {}

template <typename TSphSolver>
GravitySolver<TSphSolver>::GravitySolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& equations,
    AutoPtr<IGravity>&& gravity)
    : TSphSolver(scheduler, settings, equations)
    , gravity(std::move(gravity)) {}

template <typename TSphSolver>
GravitySolver<TSphSolver>::~GravitySolver() = default;

template <typename TSphSolver>
void GravitySolver<TSphSolver>::integrate(Storage& storage, Statistics& stats) {
    // build gravity tree first, so that the KdTree can be used in SPH
    gravity->build(this->scheduler, storage);

    // second, compute everything SPH, using the parent solver
    Timer timer;
    TSphSolver::integrate(storage, stats);
    stats.set(StatisticsId::SPH_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));

    // finally evaluate gravity for each particle
    timer.restart();
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    gravity->evalAll(this->scheduler, dv, stats);
    stats.set(StatisticsId::GRAVITY_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

template <>
const IBasicFinder& GravitySolver<AsymmetricSolver>::getFinder(ArrayView<const Vector> r) {
    RawPtr<const IBasicFinder> finder = gravity->getFinder();
    if (!finder) {
        // no finder provided, just call the default implementation
        return AsymmetricSolver::getFinder(r);
    } else {
        return *finder;
    }
}

template <>
const IBasicFinder& GravitySolver<EnergyConservingSolver>::getFinder(ArrayView<const Vector> r) {
    RawPtr<const IBasicFinder> finder = gravity->getFinder();
    if (!finder) {
        // no finder provided, just call the default implementation
        return EnergyConservingSolver::getFinder(r);
    } else {
        return *finder;
    }
}

template <>
const IBasicFinder& GravitySolver<SymmetricSolver>::getFinder(ArrayView<const Vector> UNUSED(r)) {
    // symmetric solver currently does not use this, we just implement it to make the templates work ...
    NOT_IMPLEMENTED;
}

template <typename TSphSolver>
void GravitySolver<TSphSolver>::sanityCheck(const Storage& storage) const {
    TSphSolver::sanityCheck(storage);
    // check that we don't solve gravity twice
    /// \todo generalize for ALL solvers of gravity (some categories?)
    if (this->equations.template contains<SphericalGravityEquation>()) {
        throw InvalidSetup(
            "Cannot use SphericalGravity in GravitySolver; only one solver of gravity is allowed");
    }
}

template class GravitySolver<SymmetricSolver>;
template class GravitySolver<AsymmetricSolver>;
template class GravitySolver<EnergyConservingSolver>;

NAMESPACE_SPH_END
