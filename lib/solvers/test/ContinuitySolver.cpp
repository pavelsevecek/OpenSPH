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
#include "system/Statistics.h"
#include "utils/Approx.h"

using namespace Sph;


GlobalSettings getGlobalSettings(SolverEnum id) {
    GlobalSettings settings;
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-3f);
    settings.set(GlobalSettingsIds::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT);
    settings.set(GlobalSettingsIds::SOLVER_TYPE, id);
    return settings;
}

/// Creates a sphere with ideal gas and non-zero pressure.
std::shared_ptr<Storage> makeGassBall(const GlobalSettings& globalSettings) {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsIds::ENERGY, 1.e-4_f);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::IDEAL_GAS);
    SphericalDomain domain(Vector(0._f), 1._f);
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, globalSettings);
    conds.addBody(domain, bodySettings);
    return storage;
}

TEST_CASE("ContinuitySolver", "[solvers]") {
    GlobalSettings globalSettings = getGlobalSettings(SolverEnum::CONTINUITY_SOLVER);
    auto solver = getSolver(globalSettings);

    std::shared_ptr<Storage> storage = makeGassBall(globalSettings);


    Statistics stats;
    EulerExplicit timestepping(storage, globalSettings);
    // check integrals of motion
    Integrals integrals;

    const Vector mom0 = integrals.getTotalMomentum(*storage);
    const Vector angmom0 = integrals.getTotalAngularMomentum(*storage);
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    for (Float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(*solver, stats);
    }
    const Vector mom1 = integrals.getTotalMomentum(*storage);
    const Vector angmom1 = integrals.getTotalAngularMomentum(*storage);

    REQUIRE(mom1 == approx(Vector(0._f), 1.e-4_f));
    REQUIRE(angmom1 == approx(Vector(0._f), 1.e-4_f));

    // check that particles gained some velocity
    Float totV = 0._f;
    for (Vector& v : storage->getDt<Vector>(QuantityIds::POSITIONS)) {
        totV += getLength(v);
    }
    REQUIRE(totV > 0._f);
}
