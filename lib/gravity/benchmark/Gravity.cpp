#include "bench/Session.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "gravity/VoxelGravity.h"
#include "system/Settings.h"
#include "tests/Setup.h"

using namespace Sph;

static void benchmarkGravity(Abstract::Gravity& gravity, Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f)
        .set(BodySettingsId::ENERGY, 10._f)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    Storage storage = Tests::getGassStorage(1000, settings, 5.e3_f);

    /*ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);*/
    gravity.build(storage);
    Statistics stats;
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITIONS);
    context.log("particle count: ", dv.size());
    while (context.running()) {
        gravity.evalAll(ThreadPool::getGlobalInstance(), dv, stats);
    }
    const int approx = stats.getOr<int>(StatisticsId::GRAVITY_NODES_APPROX, 0);
    const int exact = stats.getOr<int>(StatisticsId::GRAVITY_NODES_EXACT, 0);
    const int total = stats.getOr<int>(StatisticsId::GRAVITY_NODE_COUNT, 0);
    context.log("approx: ", approx);
    context.log("exact: ", exact);
    context.log("total: ", total);
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

/*BENCHMARK("Voxel Octupole 0.5", "[gravity]", Benchmark::Context& context) {
    VoxelGravity gravity(0.5_f, MultipoleOrder::OCTUPOLE);
    benchmarkGravity(gravity, context);
}

BENCHMARK("Voxel Monopole 0.5", "[gravity]", Benchmark::Context& context) {
    VoxelGravity gravity(0.5_f, MultipoleOrder::MONOPOLE);
    benchmarkGravity(gravity, context);
}*/
