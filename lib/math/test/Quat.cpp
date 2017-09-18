#include "math/Quat.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Quaternion rotation", "[quat]") {
    Quat q(Vector(1._f, 0._f, 0._f), 0.35_f);

    REQUIRE(q.convert() == approx(AffineMatrix::rotateX(0.35_f)));
}
