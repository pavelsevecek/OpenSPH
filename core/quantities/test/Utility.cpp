#include "quantities/Utility.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "quantities/Quantity.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Utility getBoundingBox", "[utility]") {
    Array<Vector> points({
        Vector(0._f),
        Vector(1._f, 0._f, 0._f),
        Vector(0._f, 2._f, 0._f),
        Vector(0._f, 0._f, 3._f),
    });
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(points));
    storage.addAttractor(Attractor(Vector(1._f, 0._f, -1._f), Vector(0._f), 0.25_f, 1._f));
    Box box = getBoundingBox(storage, 2._f);
    REQUIRE(box == Box(Vector(0._f, -0.5_f, -1.5_f), Vector(1.5_f, 2._f, 3._f)));
}

TEST_CASE("Utility getCenterOfMass", "[utility]") {
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITION, OrderEnum::ZERO, Array<Vector>({ Vector(1._f, 0._f, 0._f) }));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 2._f);
    storage.addAttractor(Attractor(Vector(0._f, 0._f, 4._f), Vector(0._f), 3._f, 6._f));

    const Vector r_com = getCenterOfMass(storage);
    REQUIRE(r_com == Vector(0.25_f, 0._f, 3._f));
    REQUIRE(r_com[H] == 0._f);
}

TEST_CASE("Utility moveInertialFrame", "[utility]") {
    Storage storage;
    storage.insert<Vector>(
        QuantityId::POSITION, OrderEnum::FIRST, Array<Vector>({ Vector(1._f, 0._f, 0._f, 3._f) }));
    storage.getDt<Vector>(QuantityId::POSITION)[0] = Vector(0._f, -1._f, 0._f);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 2._f);
    storage.addAttractor(Attractor(Vector(0._f, 0._f, 4._f), Vector(0._f, 2._f, 1._f), 3._f, 6._f));

    moveInertialFrame(storage, Vector(1._f, 0._f, 0._f), Vector(0._f, 0._f, 2._f));

    const Vector r = storage.getValue<Vector>(QuantityId::POSITION)[0];
    REQUIRE(r == Vector(2._f, 0._f, 0._f));
    REQUIRE(r[H] == 3._f);

    const Vector v = storage.getDt<Vector>(QuantityId::POSITION)[0];
    REQUIRE(v == Vector(0._f, -1._f, 2._f));
    REQUIRE(v[H] == 0._f);

    const Attractor a = storage.getAttractors()[0];
    REQUIRE(a.position == Vector(1._f, 0._f, 4._f));
    REQUIRE(a.velocity == Vector(0._f, 2._f, 3._f));
    REQUIRE(a.radius == 3._f);
}

TEST_CASE("Utility moveToCenterOfMassSystem", "[utility]") {
    RunSettings settings;
    InitialConditions ic(settings);

    BodySettings body;
    body.set(BodySettingsId::CENTER_PARTICLES, true);
    Storage storage;
    const Vector r_com(3._f, 3._f, 2._f);
    ic.addMonolithicBody(storage, SphericalDomain(r_com, 2._f), body);

    REQUIRE(getCenterOfMass(storage) == approx(r_com));

    moveToCenterOfMassFrame(storage);
    REQUIRE(getCenterOfMass(storage) == approx(Vector(0._f)));
}
