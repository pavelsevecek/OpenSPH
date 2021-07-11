#include "sph/initial/Stellar.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

template <typename TFunc>
bool compare(const Lut<Float>& actual, const TFunc& expected, const Float eps = 1.e-3_f) {
    for (auto p : actual) {
        if (!almostEqual(p.y, expected(p.x), eps)) {
            return false;
        }
    }
    return true;
}

TEST_CASE("Lane-Emden analytical solution", "[stellar]") {
    Lut<Float> phi0 = Stellar::solveLaneEmden(0._f);
    REQUIRE(compare(phi0, [](const Float z) { return 1._f - sqr(z) / 6._f; }));

    Lut<Float> phi1 = Stellar::solveLaneEmden(1._f);
    REQUIRE(compare(phi1, [](const Float z) { return sin(z) / z; }));

    Lut<Float> phi5 = Stellar::solveLaneEmden(5._f);
    REQUIRE(compare(phi5, [](const Float z) { return pow(1._f + sqr(z) / 3._f, -0.5_f); }));
}
