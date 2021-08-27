#include "gravity/NBodySolver.h"
#include "bench/Session.h"
#include "math/rng/VectorRng.h"
#include "quantities/Iterate.h"
#include "system/Settings.h"
#include "tests/Setup.h"
#include "thread/Tbb.h"

using namespace Sph;

static void benchmarkNBody(const RunSettings& settings, Benchmark::Context& context) {
    BodySettings body;
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    Storage storage = Tests::getGassStorage(1000, body, 5.e3_f);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    VectorRng<UniformRng> rng;
    for (Size i = 0; i < v.size(); ++i) {
        r[i][H] = 50._f;
        v[i] = 2.e3_f * (2 * rng() - Vector(1));
    }

    Tbb& pool = *Tbb::getGlobalInstance();
    HardSphereSolver solver(pool, settings);
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    while (context.running()) {
        Storage state = storage.clone(VisitorEnum::ALL_BUFFERS);
        solver.integrate(storage, stats);
        solver.collide(storage, stats, 1._f);
        storage = std::move(state);
    }
}

BENCHMARK("HardSphereSolver bounce", "[nbody]", Benchmark::Context& context) {
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::ELASTIC_BOUNCE);
    settings.set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL);
    benchmarkNBody(settings, context);
}

BENCHMARK("HardSphereSolver merge", "[nbody]", Benchmark::Context& context) {
    RunSettings settings;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::PERFECT_MERGING);
    settings.set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::FORCE_MERGE);
    benchmarkNBody(settings, context);
}
