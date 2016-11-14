#include "physics/Integrals.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Total momentum", "[integrals]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    // two particles, perpendicular but moving in the same direction
    storage->template insert<QuantityKey::R>(Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) },
                                             Array<Vector>{ Vector(0._f, 2._f, 0._f), Vector(0._f, 3._f, 0._f) },
                                             Array<Vector>{ Vector(0._f, 0._f, 0._f), Vector(0._f, 0._f, 0._f) });
    storage->template insert<QuantityKey::M>(Array<Float>{5._f, 7._f});

    TotalMomentum momentum(storage);
    REQUIRE(momentum() == Vector(0._f, 31._f, 0._f));

    TotalAngularMomentum angularMomentum(storage);
    REQUIRE(angularMomentum() == Vector(0._f, 0._f, 10._f));
}
