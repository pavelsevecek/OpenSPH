#include "objects/containers/CircularArray.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("CircularArray push", "[circulararray]") {
    CircularArray<int> ar(3);
    ar.push(3);
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0] == 3);
    REQUIRE_SPH_ASSERT(ar[1]);

    ar.push(5);
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[0] == 3);
    REQUIRE(ar[1] == 5);
    REQUIRE_SPH_ASSERT(ar[2]);

    ar.push(7);
    REQUIRE(ar.size() == 3);
    REQUIRE(ar[0] == 3);
    REQUIRE(ar[1] == 5);
    REQUIRE(ar[2] == 7);
    REQUIRE_SPH_ASSERT(ar[3]);

    ar.push(9);
    REQUIRE(ar.size() == 3);
    REQUIRE(ar[0] == 5);
    REQUIRE(ar[1] == 7);
    REQUIRE(ar[2] == 9);
    REQUIRE_SPH_ASSERT(ar[3]);

    ar.push(11);
    REQUIRE(ar.size() == 3);
    REQUIRE(ar[0] == 7);
    REQUIRE(ar[1] == 9);
    REQUIRE(ar[2] == 11);
    REQUIRE_SPH_ASSERT(ar[3]);
}
