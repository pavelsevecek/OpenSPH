#include "solvers/ContinuitySolver.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/wrappers/Range.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "sph/timestepping/TimeStepping.h"

using namespace Sph;

TEST_CASE("create particles", "[basicmodel]") {
    ContinuitySolver<3> model(std::make_shared<Storage>(), GLOBAL_SETTINGS);
    Settings<BodySettingsIds> bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    Storage storage = model.createParticles(domain, bodySettings);

    const int size = storage.get<QuantityKey::R>().size();
    REQUIRE((size >= 80 && size <= 120));
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [size](auto&& array) { REQUIRE(array.size() == size); });

    ArrayView<Float> rhos, us, drhos, dus;
    tie(rhos, us)   = storage.get<QuantityKey::RHO, QuantityKey::U>();
    tie(drhos, dus) = storage.dt<QuantityKey::RHO, QuantityKey::U>();
    bool result = areAllMatching(rhos, [](const Float f) {
        return f == 2700._f; // density of 2700km/m^3
    });
    REQUIRE(result);
    /* derivatives can be nonzero here, they must be cleaned in timestepping::init()
     * result = areAllMatching(drhos, [](const Float f){
        return f == 0._f; // zero density derivative
    });
    REQUIRE(result);*/
    result = areAllMatching(us, [](const Float f) {
        return f == 0._f; // zero internal energy
    });
    REQUIRE(result);
    /*result = areAllMatching(dus, [](const Float f){
        return f == 0._f; // zero energy derivative
    });
    REQUIRE(result);*/
}

TEST_CASE("simple run", "[basicmodel]") {
    Settings<GlobalSettingsIds> globalSettings(GLOBAL_SETTINGS);
    globalSettings.set<Float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f);
    globalSettings.set<int>(GlobalSettingsIds::SPH_FINDER, int(FinderEnum::BRUTE_FORCE));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    ContinuitySolver<3> model(storage, globalSettings);

    // set initial energy to nonzero value, to get some pressure
    Settings<BodySettingsIds> bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsIds::ENERGY, 100._f * Constants::gasConstant); // 100K
    BlockDomain domain(Vector(0._f), Vector(1._f));
    *storage = model.createParticles(domain, bodySettings);


    EulerExplicit timestepping(storage, globalSettings);
    // check integrals of motion
    TotalMomentum momentum(storage);
    TotalAngularMomentum angularMomentum(storage);

    const Vector mom0    = momentum();
    const Vector angmom0 = angularMomentum();
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    for (float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(&model);
    }
    const Vector mom1     = momentum();
    const Vector angmom1  = angularMomentum();
    const Float momVar    = momentum.getVariance();
    const Float angmomVar = angularMomentum.getVariance();

    REQUIRE(Math::almostEqual(mom1, Vector(0._f), Math::sqrt(momVar)));
    REQUIRE(Math::almostEqual(angmom1, Vector(0._f), Math::sqrt(angmomVar)));

    // check that particles gained some velocity
    Float totV = 0._f;
    for (Vector& v : storage->dt<QuantityKey::R>()) {
        totV += getLength(v);
    }
    REQUIRE(totV > 0._f);
}
