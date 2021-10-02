#include "bench/Session.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "system/Settings.h"
#include "tests/Setup.h"
#include "thread/Tbb.h"

using namespace Sph;

static void benchmarkGravity(IGravity& gravity, const Size N, Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f)
        .set(BodySettingsId::ENERGY, 10._f)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    Storage storage = Tests::getGassStorage(N, settings, 5.e3_f);

    Tbb& pool = *Tbb::getGlobalInstance();
    gravity.build(pool, storage);
    Statistics stats;
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    while (context.running()) {
        std::fill(dv.begin(), dv.end(), Vector(0._f));
        gravity.evalSelfGravity(pool, dv, stats);
        Benchmark::clobberMemory();
    }
}

BENCHMARK("BruteForceGravity", "[gravity]", Benchmark::Context& context) {
    BruteForceGravity gravity;
    benchmarkGravity(gravity, 10000, context);
}

BENCHMARK("BarnesHut Octupole 0.2", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.2_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, 500000, context);
}

BENCHMARK("BarnesHut Octupole 0.5", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, 500000, context);
}

BENCHMARK("BarnesHut Octupole 0.8", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.8_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, 500000, context);
}

BENCHMARK("BarnesHut Octupole 5", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(5._f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, 500000, context);
}

BENCHMARK("BarnesHut Monopole 0.2", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.2_f, MultipoleOrder::MONOPOLE);
    benchmarkGravity(gravity, 500000, context);
}

BENCHMARK("BarnesHut Monopole 0.5", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::MONOPOLE);
    benchmarkGravity(gravity, 500000, context);
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

BENCHMARK("BarnesHut build Tbb", "[gravity]", Benchmark::Context& context) {
    BarnesHut gravity(0.5_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, *Tbb::getGlobalInstance(), context);
}
