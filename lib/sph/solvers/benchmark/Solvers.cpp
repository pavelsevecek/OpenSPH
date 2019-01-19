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
#include "timestepping/TimeStepping.h"
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

template <typename TSolver>
void test(const std::string path) {
    RunSettings settings;
    settings.set(RunSettingsId::RUN_THREAD_GRANULARITY, 10000);
    std::ofstream ofs(path);
    for (Size threadCnt = 1; threadCnt <= 16; ++threadCnt) {
        Tbb tbb(threadCnt);

        TSolver solver(tbb, settings, getStandardEquations(settings));
        BodySettings body;
        SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(5'000'000, body));
        IMaterial& material = storage->getMaterial(0);
        solver.create(*storage, material);

        PredictorCorrector integrator(storage, settings);
        Statistics stats;

        // initialize and heat up
        for (Size i = 0; i < 2; ++i) {
            std::cout << "prepare " << i << std::endl;
            integrator.step(tbb, solver, stats);
        }

        Timer timer;
        for (Size i = 0; i < 50; ++i) {
            std::cout << "step " << i << std::endl;
            integrator.step(tbb, solver, stats);
        }
        const int64_t duration = timer.elapsed(TimerUnit::MILLISECOND);
        std::cout << threadCnt << "    " << duration << std::endl;
        ofs << threadCnt << "    " << duration << std::endl;
    }
}

BENCHMARK("Thread scaling", "[solvers]", Benchmark::Context& context) {
    test<AsymmetricSolver>("scaling_asym.txt");
    test<SymmetricSolver>("scaling_sym.txt");
    test<GravitySolver<AsymmetricSolver>>("scaling_gravity.txt");
    while (context.running()) {
    }
}
