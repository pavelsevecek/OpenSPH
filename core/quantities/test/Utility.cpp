#include "quantities/Utility.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "physics/Integrals.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Utility moveToCenterOfMassSystem", "[utility]") {
    RunSettings settings;
    InitialConditions ic(settings);

    BodySettings body;
    body.set(BodySettingsId::CENTER_PARTICLES, true);
    Storage storage;
    const Vector r_com(3._f, 3._f, 2._f);
    ic.addMonolithicBody(storage, SphericalDomain(r_com, 2._f), body);


    CenterOfMass evaluator;
    REQUIRE(evaluator.evaluate(storage) == approx(r_com));

    moveToCenterOfMassFrame(
        storage.getValue<Float>(QuantityId::MASS), storage.getValue<Vector>(QuantityId::POSITION));
    REQUIRE(evaluator.evaluate(storage) == approx(Vector(0._f)));
}
