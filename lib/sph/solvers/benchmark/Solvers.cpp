#include "bench/Session.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Settings.h"
#include "tests/Setup.h"

using namespace Sph;

BENCHMARK("SymmetricSolver simpler", "[solvers]", Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getSolidStorage(1000, settings);
    SymmetricSolver solver(RunSettings::getDefaults(), getStandardEquations(RunSettings::getDefaults()));
    IMaterial& material = storage.getMaterial(0);
    solver.create(storage, material);

    Statistics stats;
    while (context.running()) {
        solver.integrate(storage, stats);
    }
}

BENCHMARK("AsymmetricSolver simpler", "[solvers]", Benchmark::Context& context) {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 100._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getSolidStorage(1000, settings);
    AsymmetricSolver solver(RunSettings::getDefaults(), getStandardEquations(RunSettings::getDefaults()));
    IMaterial& material = storage.getMaterial(0);
    solver.create(storage, material);

    Statistics stats;
    while (context.running()) {
        solver.integrate(storage, stats);
    }
}
