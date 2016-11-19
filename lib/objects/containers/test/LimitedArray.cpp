#include "objects/containers/LimitedArray.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Limited Array", "[limitedarray]") {
    REQUIRE_NOTHROW(LimitedArray<Float> ar);

    LimitedArray<Float> ar = {1._f, 2._f, 3._f, 4._f, 5._f};
    for (int i=0; i<ar.size(); ++i) {
        ar.clamp(i);
    }
    // unchanged
    REQUIRE(ar == Array<Float>({1._f, 2._f, 3._f, 4._f, 5._f}));

    ar.setBounds(Range(2.5_f, 4.5_f));
    for (int i=0; i<ar.size(); ++i) {
        ar.clamp(i);
    }
    REQUIRE(ar == Array<Float>({2.5_f, 2.5_f, 3._f, 4._f, 4.5_f}));
}
