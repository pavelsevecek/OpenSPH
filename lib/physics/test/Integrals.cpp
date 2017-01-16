#include "physics/Integrals.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Total momentum", "[integrals]") {
    Storage storage;
    // two particles, perpendicular but moving in the same direction
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, Array<Vector>{ Vector(1._f, 0._f, 0._f), Vector(0._f, 2._f, 0._f) });
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    v[0] = Vector(0._f, 2._f, 0._f);
    v[1] = Vector(0._f, 3._f, 0._f);

    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, Array<Float>{ 5._f, 7._f });

    TotalMomentum momentum;
    REQUIRE(momentum.get(storage) == Vector(0._f, 31._f, 0._f));

    TotalAngularMomentum angularMomentum;
    REQUIRE(angularMomentum.get(storage) == Vector(0._f, 0._f, 10._f));
}
