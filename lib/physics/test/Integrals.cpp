#include "physics/Integrals.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "sph/initial/Initial.h"

using namespace Sph;

TEST_CASE("Total mass", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsIds::DENSITY, 10._f);
    settings.set(BodySettingsIds::PARTICLE_COUNT, 100);

    conds.addBody(SphericalDomain(Vector(0._f), 3._f), settings);
    Integrals integrals;

    REQUIRE(almostEqual(integrals.getTotalMass(*storage), 10._f * sphereVolume(3._f), 1.e-3_f));

    conds.addBody(BlockDomain(Vector(0._f), Vector(2._f)), settings);
    REQUIRE(almostEqual(integrals.getTotalMass(*storage), 10._f * (sphereVolume(3._f) + 8._f), 1.e-3_f));
}

TEST_CASE("Total Momentum Simple", "[integrals]") {
    Storage storage;
    // two particles, perpendicular but moving in the same direction
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) });
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    v[0] = Vector(0._f, 2._f, 0._f);
    v[1] = Vector(0._f, 3._f, 0._f);

    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, Array<Float>{ 5._f, 7._f });

    // integrals in inertial frame
    Integrals integrals;
    REQUIRE(integrals.getTotalMomentum(storage) == Vector(0._f, 31._f, 0._f));
    REQUIRE(integrals.getTotalAngularMomentum(storage) == Vector(0._f, 0._f, 10._f));

    GlobalSettings settings(EMPTY_SETTINGS);
    settings.set<Float>(GlobalSettingsIds::FRAME_ANGULAR_FREQUENCY, 4._f);
    integrals = Integrals(settings);

    // integrals in rotating frame
    // negative 56, because m omega x r = 7 * (0,0,4) x (0,2,0)
    REQUIRE(integrals.getTotalMomentum(storage) == Vector(-56._f, 51._f, 0._f));
    // m r^2 omega
    REQUIRE(integrals.getTotalAngularMomentum(storage) == Vector(0._f, 0._f, 142._f));
}

TEST_CASE("Total Momentum Body", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    const Float rho0 = 5._f;
    settings.set(BodySettingsIds::DENSITY, rho0);
    settings.set(BodySettingsIds::PARTICLE_COUNT, 100000); // we need lot of particles to reasonable approximate sphere

    const Float radius = 3._f;
    const Float omega = 4._f;
    conds.addBody(SphericalDomain(Vector(0._f), radius),
        settings,
        Vector(0.2_f, 0._f, -0.1_f),
        Vector(0._f, 0._f, omega));

    Integrals integrals;
    const Float totalMass = sphereVolume(radius) * rho0;
    REQUIRE(
        almostEqual(integrals.getTotalMomentum(*storage), Vector(0.2_f, 0._f, -0.1_f) * totalMass, 5._f));

    // angular momentum of homogeneous sphere = 2/5 * M * r^2 * omega
    const Float expected = 2._f / 5._f * totalMass * sqr(radius) * 4._f;
    /// \todo use positive sign? omega x r or r x omega (basically convention only)
    const Vector angMom = integrals.getTotalAngularMomentum(*storage);
    /// \todo this is very imprecise, is it to be expected?
    REQUIRE(almostEqual(angMom, Vector(0._f, 0._f, -expected), 5._f));
}

TEST_CASE("Total Energy Simple", "[integrals]") {
    Storage storage;
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>( // positions are irrelevant here ...
        QuantityIds::POSITIONS,
        Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) });
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    v[0] = Vector(0._f, 2._f, 0._f);
    v[1] = Vector(0._f, 3._f, 0._f);

    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, Array<Float>{ 5._f, 7._f });
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::ENERGY, Array<Float>{ 3._f, 4._f });

    Integrals integrals;
    REQUIRE(integrals.getTotalKineticEnergy(storage) == 41.5_f);
    REQUIRE(integrals.getTotalInternalEnergy(storage) == 43._f);
    REQUIRE(integrals.getTotalEnergy(storage) == 84.5_f);
}


TEST_CASE("Total Energy Body", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    const Float rho0 = 5._f;
    settings.set(BodySettingsIds::DENSITY, rho0);
    settings.set(BodySettingsIds::ENERGY, 20._f); // specific energy = energy per MASS
    settings.set(BodySettingsIds::PARTICLE_COUNT, 100);

    conds.addBody(SphericalDomain(Vector(0._f), 3._f), settings, Vector(5._f, 1._f, -2._f));

    Integrals integrals;
    const Float totalMass = sphereVolume(3._f) * rho0;
    REQUIRE(almostEqual(integrals.getTotalKineticEnergy(*storage), 15._f * totalMass, 1.e-3_f));
    REQUIRE(almostEqual(integrals.getTotalInternalEnergy(*storage), 20._f * totalMass, 1.e-3_f));
    REQUIRE(almostEqual(integrals.getTotalEnergy(*storage), 35._f * totalMass, 1.e-3_f));
}
