#include "timestepping/TimeStepping.h"
#include "bench/Session.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/GravitySolver.h"
#include "sph/solvers/StandardSets.h"
#include "tests/Setup.h"
#include "thread/Tbb.h"

using namespace Sph;

template <typename Timestepping>
static void benchmarkTimestepping(const Size N, Benchmark::Context& context) {
    Tbb& tbb = *Tbb::getGlobalInstance();
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(N, body));

    RunSettings settings;
    GravitySolver<AsymmetricSolver> solver(tbb, settings, getStandardEquations(settings));

    IMaterial& material = storage->getMaterial(0);
    solver.create(*storage, material);
    Timestepping timestep(storage, settings);

    Statistics stats;
    while (context.running()) {
        timestep.step(tbb, solver, stats);
    }

#ifdef SPH_PROFILE
    StdOutLogger logger;
    Profiler::getInstance().printStatistics(logger);
#endif
}

BENCHMARK("EulerExplicit N=1e5", "[timestepping]", Benchmark::Context& context) {
    benchmarkTimestepping<EulerExplicit>(100000, context);
}

BENCHMARK("LeapFrog N=1e5", "[timestepping]", Benchmark::Context& context) {
    benchmarkTimestepping<LeapFrog>(100000, context);
}

BENCHMARK("PredictorCorrector N=1e5", "[timestepping]", Benchmark::Context& context) {
    benchmarkTimestepping<PredictorCorrector>(100000, context);
}

BENCHMARK("EulerExplicit N=1e6", "[timestepping]", Benchmark::Context& context) {
    benchmarkTimestepping<EulerExplicit>(1000000, context);
}

BENCHMARK("LeapFrog N=1e6", "[timestepping]", Benchmark::Context& context) {
    benchmarkTimestepping<LeapFrog>(1000000, context);
}

BENCHMARK("PredictorCorrector N=1e6", "[1000000]", Benchmark::Context& context) {
    benchmarkTimestepping<PredictorCorrector>(100000, context);
}
