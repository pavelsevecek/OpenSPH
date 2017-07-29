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
    settings.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getGassStorage(10000, settings);

    /*ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);*/
    gravity.build(storage);
    Statistics stats;
    while (context.running()) {
        // MinMaxMen approx, exact;
        gravity.evalAll(storage.getD2t<Vector>(QuantityId::POSITIONS), stats);
        /*if (stats.has(StatisticsId::GRAVITY_PARTICLES_EXACT)) {
           const int approx = stats.get<int>(StatisticsId::GRAVITY_PARTICLES_APPROX);
            const int exact = stats.get<int>(StatisticsId::GRAVITY_PARTICLES_EXACT);
            ASSERT(approx + exact == int(r.size()), approx + exact, r.size());
            /// \todo UNIT TEST !!!!
            // approx.accumulate();
            // exact.accumulate();
        }*/
    }
    //        context.log("approx: ", approx);
    //        context.log("exact: ", exact);
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
