#include "sph/solvers/ContinuitySolver.h"
#include "bench/Session.h"
#include "system/Settings.h"
#include "tests/Setup.h"

using namespace Sph;

BENCHMARK("ContinuitySolver simpler", "[solvers]", Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getSolidStorage(1000, settings);
    ContinuitySolver solver(RunSettings::getDefaults());
    Abstract::Material& material = storage.getMaterial(0);
    solver.create(storage, material);

    Statistics stats;
    while (context.running()) {
        solver.integrate(storage, stats);
    }
}
