#include "bench/Session.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "system/Settings.h"
#include "tests/Setup.h"
#include "thread/Tbb.h"

using namespace Sph;

static void benchmarkGravity(IGravity& gravity, Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f)
        .set(BodySettingsId::ENERGY, 10._f)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    Storage storage = Tests::getGassStorage(100000, settings, 5.e3_f);

    /*ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);*/
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    gravity.build(pool, storage);
    Statistics stats;
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    // context.log("particle count: ", dv.size());
    while (context.running()) {
        std::fill(dv.begin(), dv.end(), Vector(0._f));
        gravity.evalAll(pool, dv, stats);
        Benchmark::clobberMemory();
    }
    /*const int approx = stats.getOr<int>(StatisticsId::GRAVITY_NODES_APPROX, 0);
    const int exact = stats.getOr<int>(StatisticsId::GRAVITY_NODES_EXACT, 0);
    const int total = stats.getOr<int>(StatisticsId::GRAVITY_NODE_COUNT, 0);
    context.log("approx: ", approx);
    context.log("exact: ", exact);
    context.log("total: ", total);*/
}

BENCHMARK("BruteForceGravity", "[gravity]", Benchmark::Context& context) {
    BruteForceGravity gravity;
    benchmarkGravity(gravity, context);
}

BENCHMARK("BarnesHut Octupole 0.2", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.2_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, context);
}

BENCHMARK("BarnesHut Octupole 0.5", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, context);
}

BENCHMARK("BarnesHut Octupole 0.8", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.8_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, context);
}

BENCHMARK("BarnesHut Octupole 5", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(5._f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, context);
}

/*BENCHMARK("BarnesHut Monopole 0.2", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.2_f, MultipoleOrder::MONOPOLE);
    benchmarkGravity(gravity, context);
}*/

BENCHMARK("BarnesHut Monopole 0.5", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::MONOPOLE);
    benchmarkGravity(gravity, context);
}

static void benchmarkGravity(IGravity& gravity, IScheduler& scheduler, Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getGassStorage(1000000, settings, 5.e3_f);

    while (context.running()) {
        gravity.build(scheduler, storage);
        Benchmark::clobberMemory();
    }
}

BENCHMARK("BarnesHut build Sequential", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, SEQUENTIAL, context);
}

BENCHMARK("BarnesHut build ThreadPool", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, *ThreadPool::getGlobalInstance(), context);
}
