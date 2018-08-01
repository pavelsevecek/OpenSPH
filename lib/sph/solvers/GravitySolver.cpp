#include "sph/solvers/GravitySolver.h"
#include "gravity/SphericalGravity.h"
#include "sph/equations/Potentials.h"
#include "sph/kernel/Kernel.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Factory.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

template <typename TSphSolver>
GravitySolver<TSphSolver>::GravitySolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& equations)
    : GravitySolver(scheduler, settings, equations, Factory::getGravity(settings)) {}

template <>
GravitySolver<SymmetricSolver>::GravitySolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& equations,
    AutoPtr<IGravity>&& gravity)
    : SymmetricSolver(scheduler, settings, equations)
    , gravity(std::move(gravity)) {

    // make sure acceleration are being accumulated
    for (ThreadData& data : threadData) {
        Accumulated& results = data.derivatives.getAccumulated();
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
    }
}

template <>
GravitySolver<AsymmetricSolver>::GravitySolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& equations,
    AutoPtr<IGravity>&& gravity)
    : AsymmetricSolver(scheduler, settings, equations)
    , gravity(std::move(gravity)) {

    // make sure acceleration are being accumulated
    Accumulated& results = derivatives.getAccumulated();
    results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
}

template <typename TSphSolver>
void GravitySolver<TSphSolver>::loop(Storage& storage, Statistics& stats) {
    // first, do asymmetric evaluation of gravity:

    // build gravity tree
    gravity->build(this->scheduler, storage);

    // get acceleration buffer corresponding to first thread (to save some memory + time)
    Accumulated& accumulated = this->getAccumulated();
    ArrayView<Vector> dv = accumulated.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);

    // evaluate gravity for each particle
    Timer timer;
    gravity->evalAll(this->scheduler, dv, stats);
    stats.set(StatisticsId::GRAVITY_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));

    // second, compute SPH derivatives using given solver
    timer.restart();
    TSphSolver::loop(storage, stats);
    stats.set(StatisticsId::SPH_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

template <>
Accumulated& GravitySolver<SymmetricSolver>::getAccumulated() {
    // gravity is evaluated asymmetrically, so we can simply put everything in the first (or any) accumulated
    ThreadData& data = this->threadData.value(0);
    return data.derivatives.getAccumulated();
}

template <>
Accumulated& GravitySolver<AsymmetricSolver>::getAccumulated() {
    return this->derivatives.getAccumulated();
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

NAMESPACE_SPH_END
