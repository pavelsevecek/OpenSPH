#include "physics/Integrals.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "utils/Approx.h"

using namespace Sph;

TEST_CASE("Total mass", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsIds::DENSITY, 10._f);
    settings.set(BodySettingsIds::PARTICLE_COUNT, 100);

    conds.addBody(SphericalDomain(Vector(0._f), 3._f), settings);
    TotalMass mass;

    REQUIRE(mass.evaluate(*storage) == approx(10._f * sphereVolume(3._f), 1.e-3_f));

    conds.addBody(BlockDomain(Vector(0._f), Vector(2._f)), settings);
    REQUIRE(mass.evaluate(*storage) == approx(10._f * (sphereVolume(3._f) + 8._f), 1.e-3_f));
}

TEST_CASE("Total Momentum Simple", "[integrals]") {
    Storage storage;
    // two particles, perpendicular but moving in the same direction
    storage.insert<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) });
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    v[0] = Vector(0._f, 2._f, 0._f);
    v[1] = Vector(0._f, 3._f, 0._f);

    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, Array<Float>{ 5._f, 7._f });

    // integrals in inertial frame
    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;
    REQUIRE(momentum.evaluate(storage) == Vector(0._f, 31._f, 0._f));
    REQUIRE(angularMomentum.evaluate(storage) == Vector(0._f, 0._f, 10._f));

    momentum = TotalMomentum(4._f);
    angularMomentum = TotalAngularMomentum(4._f);
    // integrals in rotating frame
    // negative 56, because m omega x r = 7 * (0,0,4) x (0,2,0)
    REQUIRE(momentum.evaluate(storage) == Vector(-56._f, 51._f, 0._f));
    // m r^2 omega
    REQUIRE(angularMomentum.evaluate(storage) == Vector(0._f, 0._f, 142._f));
}

TEST_CASE("Total Momentum Body", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    const Float rho0 = 5._f;
    settings.set(BodySettingsIds::DENSITY, rho0);
    settings.set(
        BodySettingsIds::PARTICLE_COUNT, 100000); // we need lot of particles to reasonable approximate sphere

    const Float radius = 3._f;
    const Float omega = 4._f;
    conds.addBody(SphericalDomain(Vector(0._f), radius),
        settings,
        Vector(0.2_f, 0._f, -0.1_f),
        Vector(0._f, 0._f, omega));

    TotalMomentum momentum;
    const Float totalMass = sphereVolume(radius) * rho0;
    REQUIRE(momentum.evaluate(*storage) == approx(Vector(0.2_f, 0._f, -0.1_f) * totalMass, 1.e-3_f));

    // angular momentum of homogeneous sphere = 2/5 * M * r^2 * omega
    const Float expected = 2._f / 5._f * totalMass * sqr(radius) * 4._f;
    /// \todo use positive sign? omega x r or r x omega (basically convention only)
    const Vector angMom = TotalAngularMomentum().evaluate(*storage);
    /// \todo this is very imprecise, is it to be expected?
    REQUIRE(angMom == approx(Vector(0._f, 0._f, -expected), 1.e-2_f));
}

TEST_CASE("Total Energy Simple", "[integrals]") {
    Storage storage;
    storage.insert<Vector, OrderEnum::SECOND_ORDER>( // positions are irrelevant here ...
        QuantityIds::POSITIONS,
        Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) });
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    v[0] = Vector(0._f, 2._f, 0._f);
    v[1] = Vector(0._f, 3._f, 0._f);

    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, Array<Float>{ 5._f, 7._f });
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::ENERGY, Array<Float>{ 3._f, 4._f });

    REQUIRE(TotalKineticEnergy().evaluate(storage) == 41.5_f);
    REQUIRE(TotalInternalEnergy().evaluate(storage) == 43._f);
    REQUIRE(TotalEnergy().evaluate(storage) == 84.5_f);
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

    const Float totalMass = sphereVolume(3._f) * rho0;
    REQUIRE(TotalKineticEnergy().evaluate(*storage) == approx(15._f * totalMass));
    REQUIRE(TotalInternalEnergy().evaluate(*storage) == approx(20._f * totalMass));
    REQUIRE(TotalEnergy().evaluate(*storage) == approx(35._f * totalMass));
}

TEST_CASE("Center of Mass", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsIds::DENSITY, 1000._f);
    const Vector r1(-1._f, 5._f, -2._f);
    conds.addBody(BlockDomain(r1, Vector(1._f)), settings);
    settings.set(BodySettingsIds::DENSITY, 500._f);
    const Vector r2(5._f, 3._f, 1._f);
    conds.addBody(BlockDomain(r2, Vector(2._f)), settings);

    REQUIRE(CenterOfMass(0).evaluate(*storage) == approx(r1));
    REQUIRE(CenterOfMass(1).evaluate(*storage) == approx(r2));

    // second body is 8x bigger in volume, but half the density -> 4x more massive
    REQUIRE(CenterOfMass().evaluate(*storage) == approx((r1 + 4._f * r2) / 5._f));
}

TEST_CASE("IntegralWrapper", "[integrals]") {
    Storage storage;
    storage.insert<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) });
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, 1._f);
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::ENERGY, 0._f);
    IntegralWrapper w1 = TotalEnergy(0._f);
    IntegralWrapper w2 = TotalMomentum(0._f);
    Value v1 = w1.evaluate(storage);
    Value v2 = w2.evaluate(storage);
}

TEST_CASE("Pairing", "[diagnostics]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings settings;
    settings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    conds.addBody(SphericalDomain(Vector(0._f), 3._f), settings);
    Array<Vector>& r = storage->getValue<Vector>(QuantityIds::POSITIONS);

    Diagnostics diag(GlobalSettings::getDefaults());
    REQUIRE(diag.getParticlePairs(*storage, 1.e-1_f).empty());

    const Size n = r.size();
    r.push(r[55]); // -> pair n, 55
    r.push(r[68]); // -> pair n+1, 68
    r.push(r[12]); // -> pair n+2, 12

    Array<Diagnostics::Pair> pairs = diag.getParticlePairs(*storage);
    REQUIRE(pairs.size() == 3);
    std::sort(pairs.begin(), pairs.end(), [](auto&& p1, auto&& p2) {
        // sort by lower indices
        return min(p1.i1, p1.i2) < min(p2.i1, p2.i2);
    });
    REQUIRE(min(pairs[0].i1, pairs[0].i2) == 12);
    REQUIRE(max(pairs[0].i1, pairs[0].i2) == n + 2);
    REQUIRE(min(pairs[1].i1, pairs[1].i2) == 55);
    REQUIRE(max(pairs[1].i1, pairs[1].i2) == n);
    REQUIRE(min(pairs[2].i1, pairs[2].i2) == 68);
    REQUIRE(max(pairs[2].i1, pairs[2].i2) == n + 1);
}
