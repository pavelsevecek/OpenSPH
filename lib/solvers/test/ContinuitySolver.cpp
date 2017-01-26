#include "solvers/ContinuitySolver.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/wrappers/Range.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "solvers/SolverFactory.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"

using namespace Sph;


TEST_CASE("ContinuitySolver", "[solvers]") {
    GlobalSettings globalSettings;
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f);
    globalSettings.set(GlobalSettingsIds::SPH_FINDER, int(FinderEnum::BRUTE_FORCE));
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false);
    auto solver = getSolver(globalSettings);

    // set initial energy to nonzero value, to get some pressure
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsIds::ENERGY, 1.e-4_f);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, globalSettings);
    conds.addBody(domain, bodySettings);

    Statistics stats;
    EulerExplicit timestepping(storage, globalSettings);
    // check integrals of motion
    Integrals integrals;

    const Vector mom0 = integrals.getTotalMomentum(*storage);
    const Vector angmom0 = integrals.getTotalAngularMomentum(*storage);
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    for (float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(*solver, stats);
    }
    const Vector mom1 = integrals.getTotalMomentum(*storage);
    const Vector angmom1 = integrals.getTotalAngularMomentum(*storage);

    REQUIRE(almostEqual(mom1, Vector(0._f)));
    REQUIRE(almostEqual(angmom1, Vector(0._f)));

    // check that particles gained some velocity
    Float totV = 0._f;
    for (Vector& v : storage->getAll<Vector>(QuantityIds::POSITIONS)[1]) {
        totV += getLength(v);
    }
    REQUIRE(totV > 0._f);
}
