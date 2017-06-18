#include "sph/solvers/ContinuitySolver.h"
#include "bench/Session.h"
#include "system/Settings.h"
#include "tests/Setup.h"

using namespace Sph;

BENCHMARK("ContinuitySolver simpler", "[solvers]", Benchmark::Context& context) {
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults());
    ContinuitySolver solver(RunSettings::getDefaults());
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    while (context.running()) {
        solver.integrate(storage, stats);
    }
}
