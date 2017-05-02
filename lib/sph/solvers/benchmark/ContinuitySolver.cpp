#include "benchmark/benchmark_api.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "system/Settings.h"

using namespace Sph;

template <typename TGlobalSettingsOverride, typename TBodySettingsOverride>
static void continuitySolverRun(benchmark::State& state,
    TGlobalSettingsOverride gso,
    TBodySettingsOverride bso) {
    GlobalSettings globalSettings;
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, false);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false);
    globalSettings.set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL);
    gso(globalSettings);
    Problem* p = new Problem(globalSettings);
    p->timeRange = Range(0._f, 3e-6f);
    p->timeStepping = Factory::getTimestepping(globalSettings, p->storage);

    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::ENERGY, 1.e-6_f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 1000);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
    bso(bodySettings);
    InitialConditions conds(p->storage, globalSettings);

    SphericalDomain domain1(Vector(0._f), 5e2_f); // D = 1km
    conds.addBody(domain1, bodySettings);
    SphericalDomain domain2(Vector(5.4e2_f, 1.35e2_f, 0._f), 20._f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f));
    while (state.KeepRunning()) {
        p->run();
    }
}

static void baselineRun(benchmark::State& state) {
    continuitySolverRun(state, [](GlobalSettings&) {}, [](BodySettings&) {});
}
BENCHMARK(baselineRun);

static void sortedRun(benchmark::State& state) {
    continuitySolverRun(state,
        [](GlobalSettings&) {},
        [](BodySettings& bs) { bs.set(BodySettingsIds::PARTICLE_SORTING, true); });
}
BENCHMARK(sortedRun);
