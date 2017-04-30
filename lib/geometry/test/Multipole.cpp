#include "geometry/Multipole.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Multipole", "[multipole]") {
    Multipole<2> m1(Vector(2._f, 1._f, -1._f), Vector(1._f, 0._f, 3._f), Vector(-3._f, 2._f, 2._f));
    Multipole<2> m2(Vector(1._f, 0._f, 2._f), Vector(2._f, -1._f, 1._f), Vector(5._f, -2._f, 3._f));

    Multipole<2> m3 = m1 + m2;
    REQUIRE(m3[0] == Multipole<1>(3._f, 1._f, 1._f));
    REQUIRE(m3[1] == Multipole<1>(3._f, -1._f, 4._f));
    REQUIRE(m3[2] == Multipole<1>(2._f, 0._f, 5._f));
}
