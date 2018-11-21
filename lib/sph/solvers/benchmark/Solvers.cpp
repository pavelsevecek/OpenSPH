#include "bench/Session.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/EnergyConservingSolver.h"
#include "sph/solvers/GravitySolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Platform.h"
#include "system/Settings.h"
#include "tests/Setup.h"
#include "thread/Tbb.h"
#include <atomic>
#include <iostream>

using namespace Sph;

static void benchmarkSolver(ISolver& solver, Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getSolidStorage(2000000, settings);
    IMaterial& material = storage.getMaterial(0);
    solver.create(storage, material);

    Statistics stats;
    while (context.running()) {
        solver.integrate(storage, stats);
    }

#ifdef SPH_PROFILE
    StdOutLogger logger;
    Profiler::getInstance().printStatistics(logger);
#endif
}

BENCHMARK("SymmetricSolver simple", "[solvers]", Benchmark::Context& context) {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    SymmetricSolver solver(pool, settings, getStandardEquations(settings));
    benchmarkSolver(solver, context);
}

BENCHMARK("AsymmetricSolver simple", "[solvers]", Benchmark::Context& context) {
    RunSettings settings;
    IScheduler& pool = *ThreadPool::getGlobalInstance();
    AsymmetricSolver solver(pool, settings, getStandardEquations(settings));
    benchmarkSolver(solver, context);
}

BENCHMARK("EnergyConservingSolver simple", "[solvers]", Benchmark::Context& context) {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    EnergyConservingSolver solver(pool, settings, getStandardEquations(settings));
    benchmarkSolver(solver, context);
}

BENCHMARK("GravitySolver simple", "[solvers]", Benchmark::Context& context) {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    GravitySolver<AsymmetricSolver> solver(pool, settings, getStandardEquations(settings));
    benchmarkSolver(solver, context);
}
