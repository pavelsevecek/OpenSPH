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
    GlobalSettings globalSettings(GLOBAL_SETTINGS);
    globalSettings.set<Float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f);
    globalSettings.set<int>(GlobalSettingsIds::SPH_FINDER, int(FinderEnum::BRUTE_FORCE));
    auto solver = getSolver(globalSettings);

    // set initial energy to nonzero value, to get some pressure
    BodySettings bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsIds::ENERGY, 100._f * Constants::gasConstant); // 100K
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>(bodySettings);
    InitialConditions conds(storage, globalSettings);
    conds.addBody(domain, bodySettings);

    EulerExplicit timestepping(storage, globalSettings);
    // check integrals of motion
    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;

    const Vector mom0 = momentum(*storage);
    const Vector angmom0 = angularMomentum(*storage);
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    for (float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(*solver);
    }
    const Vector mom1 = momentum(*storage);
    const Vector angmom1 = angularMomentum(*storage);
    const Float momVar = momentum.getVariance(*storage);
    const Float angmomVar = angularMomentum.getVariance(*storage);

    REQUIRE(Math::almostEqual(mom1, Vector(0._f), Math::sqrt(momVar)));
    REQUIRE(Math::almostEqual(angmom1, Vector(0._f), Math::sqrt(angmomVar)));

    // check that particles gained some velocity
    Float totV = 0._f;
    for (Vector& v : storage->getAll<Vector>(QuantityKey::POSITIONS)[1]) {
        totV += getLength(v);
    }
    REQUIRE(totV > 0._f);
}
