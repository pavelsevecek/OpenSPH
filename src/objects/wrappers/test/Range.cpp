#include "objects/wrappers/Range.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Range", "[range]") {
    Range<float> range;
    REQUIRE(!range.contains(0.f));
    REQUIRE(!range.contains(INFTY));
    REQUIRE(!range.contains(-INFTY));

    range.extend(0.f);
    REQUIRE(range.contains(0.f));
    REQUIRE(!range.contains(1.f));
    REQUIRE(!range.contains(-1.f));

    range.extend(1.f);
    REQUIRE(range.contains(0.f));
    REQUIRE(range.contains(1.f));
    REQUIRE(!range.contains(2.f));
    REQUIRE(!range.contains(-1.f));

    REQUIRE(range.clamp(2.f) == 1.f);
    REQUIRE(range.clamp(1.f) == 1.f);
    REQUIRE(range.clamp(0.5f) == 0.5f);
    REQUIRE(range.clamp(0.f) == 0.f);
    REQUIRE(range.clamp(-0.5f) == 0.f);
}


TEST_CASE("Range loop", "[range]") {
    Range<float> range(0.f, 5.f);
    auto adapter = rangeAdapter(range, 1.f);
    REQUIRE(*(adapter.begin()) == 0.f);
    REQUIRE(*(adapter.end()) == 5.f);

    float array[] = {0.f, 1.f, 2.f, 3.f, 4.f};

    int idx = 0;
    for (float& i : adapter) {
        REQUIRE(i == array[idx++]);
    }

    float step = 0.5f;
    Range<float> range2(0.f, 20.f);
    auto adapter2 = rangeAdapter(range2, step);
    REQUIRE(*(adapter2.begin()) == 0.f);
    REQUIRE(*(adapter2.end()) == 20.f);

    float array2[] = {0.f, 1.f, 3.f, 7.f, 15.f};
    idx = 0;
    for (float& i : adapter2) {
        REQUIRE(i == array2[idx++]);
        step *= 2.f;
    }

}
