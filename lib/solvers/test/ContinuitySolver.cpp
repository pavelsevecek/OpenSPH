#include "solvers/ContinuitySolver.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/wrappers/Range.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "sph/timestepping/TimeStepping.h"

using namespace Sph;

/// \todo
/*
TEST_CASE("create particles", "[basicmodel]") {
    ContinuitySolver<3, Force> model(GLOBAL_SETTINGS);
    Settings<BodySettingsIds> bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    Storage storage;
    model.setQuantities(storage, domain, bodySettings);

    const int size = storage.getValue<Vector>(QuantityKey::R).size();
    REQUIRE((size >= 80 && size <= 120));
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [size](auto&& array) { REQUIRE(array.size() == size); });

    ArrayView<Float> rhos, us, drhos, dus;
    tieToArray(rhos, drhos) = storage.getAll<Float>(QuantityKey::RHO);
    tieToArray(us, dus) = storage.getAll<Float>(QuantityKey::U);
    bool result = areAllMatching(rhos, [](const Float f) {
        return f == 2700._f; // density of 2700km/m^3
    });
    REQUIRE(result);

    result = areAllMatching(drhos, [](const Float f) {
        return f == 0._f; // zero density derivative
    });
    REQUIRE(result);
    result = areAllMatching(us, [](const Float f) {
        return f == 0._f; // zero internal energy
    });
    REQUIRE(result);
    result = areAllMatching(dus, [](const Float f) {
        return f == 0._f; // zero energy derivative
    });
    REQUIRE(result);

    ArrayView<Float> ms = storage.getValue<Float>(QuantityKey::M);
    float totalM = 0._f;
    for (Float m : ms) {
        totalM += m;
    }
    REQUIRE(Math::almostEqual(totalM, 2700._f * domain.getVolume()));
}

TEST_CASE("simple run", "[basicmodel]") {
    GlobalSettings globalSettings(GLOBAL_SETTINGS);
    globalSettings.set<Float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f);
    globalSettings.set<int>(GlobalSettingsIds::SPH_FINDER, int(FinderEnum::BRUTE_FORCE));

    using Force = PressureForce<StandardAV>;
    ContinuitySolver<3, Force> model(globalSettings);

    // set initial energy to nonzero value, to get some pressure
    Settings<BodySettingsIds> bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsIds::ENERGY, 100._f * Constants::gasConstant); // 100K
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>(bodySettings);
    model.setQuantities(*storage, domain, bodySettings);


    EulerExplicit timestepping(storage, globalSettings);
    // check integrals of motion
    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;

    const Vector mom0 = momentum(*storage);
    const Vector angmom0 = angularMomentum(*storage);
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    for (float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(model);
    }
    const Vector mom1 = momentum(*storage);
    const Vector angmom1 = angularMomentum(*storage);
    const Float momVar = momentum.getVariance(*storage);
    const Float angmomVar = angularMomentum.getVariance(*storage);

    REQUIRE(Math::almostEqual(mom1, Vector(0._f), Math::sqrt(momVar)));
    REQUIRE(Math::almostEqual(angmom1, Vector(0._f), Math::sqrt(angmomVar)));

    // check that particles gained some velocity
    Float totV = 0._f;
    for (Vector& v : storage->getAll<Vector>(QuantityKey::R)[1]) {
        totV += getLength(v);
    }
    REQUIRE(totV > 0._f);
}
*/
