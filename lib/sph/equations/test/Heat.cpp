#include "sph/equations/Heat.h"
#include "catch.hpp"
#include "io/Output.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "system/Factory.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("Heat Diffusion simple", "[heat]") {
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 10._f).set(BodySettingsId::ENERGY, 10._f);
    Storage storage = Tests::getGassStorage(100, body);
    HeatDiffusionEquation eq;
    REQUIRE_NOTHROW(eq.create(storage, storage.getMaterial(0)));
    REQUIRE_NOTHROW(eq.initialize(SEQUENTIAL, storage, 0._f));
    REQUIRE_NOTHROW(eq.finalize(SEQUENTIAL, storage, 0._f));
}

TEST_CASE("Heat Diffusion 1D", "[heat]") {
    const Float size = 100._f;
    const Float u1 = 100._f;
    const Float u2 = 1000._f;
    const Float alpha = 10._f;

    BlockDomain domain(Vector(0._f), Vector(size, 1._f, 1._f));
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 10._f)
        .set(BodySettingsId::ENERGY, u1)
        .set(BodySettingsId::ENERGY_MIN, 10._f)
        .set(BodySettingsId::DIFFUSIVITY, alpha);
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getGassStorage(1000, body, domain));
    ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
    ArrayView<const Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        if (r[i][X] > 0._f) {
            u[i] = u2;
        }
    }

    EquationHolder eqs;
    eqs += makeTerm<HeatDiffusionEquation>();
    eqs += makeTerm<ConstSmoothingLength>();

    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1._f);
    IScheduler& scheduler = *Factory::getScheduler();
    AsymmetricSolver solver(scheduler, settings, eqs);
    EulerExplicit stepper(storage, settings);
    solver.create(*storage, storage->getMaterial(0));

    Statistics stats;
    const Float t_end = 1._f;
    for (Float t = 0; t < t_end; t += stepper.getTimeStep()) {
        stepper.step(scheduler, solver, stats);
    }

    auto solution = [u1, u2, alpha, t_end](const Float x) {
        return 0.5_f * (u1 + u2) + 0.5_f * (u2 - u1) * std::erf(x / sqrt(4._f * alpha * t_end));
    };

    auto test = [&](const Size i) -> Outcome {
        const Float u0 = solution(r[i][X]);
        if (abs(u[i] - u0) > 20._f) { // absolute difference rather than approx (relative)
            return makeFailed("Incorrect solution: ", u[i], " == ", u0);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());

    /*TextOutput output(Path("hde.txt"), "hde", OutputQuantityFlag::POSITION | OutputQuantityFlag::ENERGY);
    output.dump(*storage, stats);*/
}
