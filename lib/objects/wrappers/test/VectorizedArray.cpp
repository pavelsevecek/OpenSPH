#include "objects/wrappers/VectorizedArray.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Vectorized array", "[vectorizedarray]") {
    Array<float> array { 1._f, 2._f, 3._f, 4._f, 5._f, 6._f, 7._f, 8._f};

    VectorizedArray vectorized(array);
    REQUIRE(vectorized->size() == 2);

    vectorized.get()[1] -= vectorized.get()[0];
    for (int i=3; i<8; ++i) {
        REQUIRE(array[i] == 4._f);
    }
}
