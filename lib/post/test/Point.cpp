#include "post/Point.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("AffineMatrix2 Inverse", "[point]") {
    AffineMatrix2 id;
    REQUIRE(id == id.inverse());

    // translation matrix
    AffineMatrix2 trans(1._f, PlotPoint(6._f, -3._f));
    AffineMatrix2 transInv = trans.inverse();
    REQUIRE(transInv == AffineMatrix2(1._f, PlotPoint(-6._f, 3._f)));

    // generic transformation
    AffineMatrix2 m(2._f, -3._f, -0.5_f, 6._f, 4._f, -1._f);
    REQUIRE(m != m.inverse());
    for (Size i = 0; i < 2; ++i) {
        for (Size j = 0; j < 3; ++j) {
            REQUIRE(m(i, j) == approx(m.inverse().inverse()(i, j)));
        }
    }
}
