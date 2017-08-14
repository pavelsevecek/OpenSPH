#include "objects/geometry/Multipole.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Multipole", "[multipole]") {
    static_assert(Detail::MultipoleMapping<X>::value == 0, "static test failed");
    static_assert(Detail::MultipoleMapping<Y>::value == 1, "static test failed");
    static_assert(Detail::MultipoleMapping<Z>::value == 2, "static test failed");

    static_assert(Detail::MultipoleMapping<X, X, X>::value == 0, "static test failed");
    static_assert(Detail::MultipoleMapping<X, X, Y>::value == 1, "static test failed");
    static_assert(Detail::MultipoleMapping<Y, X, X>::value == 1, "static test failed");
    static_assert(Detail::MultipoleMapping<X, X, Z>::value == 2, "static test failed");
    static_assert(Detail::MultipoleMapping<X, Z, X>::value == 2, "static test failed");
    static_assert(Detail::MultipoleMapping<Z, X, X>::value == 2, "static test failed");
    static_assert(Detail::MultipoleMapping<X, Y, Y>::value == 3, "static test failed");
    static_assert(Detail::MultipoleMapping<X, Y, Z>::value == 4, "static test failed");
    static_assert(Detail::MultipoleMapping<X, Z, Z>::value == 5, "static test failed");
    static_assert(Detail::MultipoleMapping<Y, Y, Y>::value == 6, "static test failed");
    static_assert(Detail::MultipoleMapping<Y, Y, Z>::value == 7, "static test failed");
    static_assert(Detail::MultipoleMapping<Z, Y, Y>::value == 7, "static test failed");
    static_assert(Detail::MultipoleMapping<Y, Z, Z>::value == 8, "static test failed");
    static_assert(Detail::MultipoleMapping<Z, Z, Z>::value == 9, "static test failed");

    MomentOperators::Delta<2> delta;

    MomentOperators::Contraction<MomentOperators::Delta<2>> contract{ delta };
    REQUIRE(contract.value() == 3);

    /*
    Multipole<2> m1(Vector(2._f, 1._f, -1._f), Vector(1._f, 0._f, 3._f), Vector(-3._f, 2._f, 2._f));
    Multipole<2> m2(Vector(1._f, 0._f, 2._f), Vector(2._f, -1._f, 1._f), Vector(5._f, -2._f, 3._f));

    Multipole<2> m3 = m1 + m2;
    REQUIRE(m3[0] == Multipole<1>(3._f, 1._f, 1._f));
    REQUIRE(m3[1] == Multipole<1>(3._f, -1._f, 4._f));
    REQUIRE(m3[2] == Multipole<1>(2._f, 0._f, 5._f));*/
}

TEST_CASE("TracelessMultipole", "[multipole]") {
    static_assert(Detail::TracelessMultipoleMapping<X>::value == 0, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Y>::value == 1, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Z>::value == 2, "static test failed");

    static_assert(Detail::TracelessMultipoleMapping<X, X, X>::value == 0, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<X, X, Y>::value == 1, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<X, X, Z>::value == 2, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<X, Y, Y>::value == 3, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<X, Y, Z>::value == 4, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Z, Y, X>::value == 4, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Z, X, Y>::value == 4, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Y, Y, Y>::value == 5, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Y, Y, Z>::value == 6, "static test failed");
    static_assert(Detail::TracelessMultipoleMapping<Z, Y, Y>::value == 6, "static test failed");
}
