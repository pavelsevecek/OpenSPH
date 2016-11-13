#include "objects/containers/BufferedArray.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Buffered Array", "[bufferedarray]") {
    BufferedArray<int> ar(Array<int>{ 5, 4, 3, 2, 1});

    REQUIRE(ar.first() == Array<int>({ 5, 4, 3, 2, 1 }));
    ar.swap();
    REQUIRE(ar->size() == 0);
    ar = Array<int>{1, 2, 3, 4};
    ar.swap();
    REQUIRE(ar.first() == Array<int>({ 5, 4, 3, 2, 1 }));
    ar.swap();
    REQUIRE(ar.first() == Array<int>({ 1, 2, 3, 4 }));

    BufferedArray<int> ar2 = std::move(ar);
    REQUIRE(ar2.first() == Array<int>({ 1, 2, 3, 4 }));
    ar2.swap();
    REQUIRE(ar2.first() == Array<int>({ 5, 4, 3, 2, 1 }));
}
