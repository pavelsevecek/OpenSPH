#include "objects/wrappers/Range.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Range", "[range]") {
    Range range;
    REQUIRE(!range.contains(0._f));
    REQUIRE(!range.contains(LARGE));
    REQUIRE(!range.contains(-LARGE));

    range.extend(0._f);
    REQUIRE(range.contains(0._f));
    REQUIRE(!range.contains(1._f));
    REQUIRE(!range.contains(-1._f));

    range.extend(1._f);
    REQUIRE(range.contains(0._f));
    REQUIRE(range.contains(1._f));
    REQUIRE(!range.contains(2._f));
    REQUIRE(!range.contains(-1._f));

    REQUIRE(range.clamp(2._f) == 1._f);
    REQUIRE(range.clamp(1._f) == 1._f);
    REQUIRE(range.clamp(0.5_f) == 0.5_f);
    REQUIRE(range.clamp(0._f) == 0._f);
    REQUIRE(range.clamp(-0.5_f) == 0._f);
}

TEST_CASE("One sided range", "[range]") {
    Range range1(1._f, INFTY);
    REQUIRE(!range1.contains(-LARGE));
    REQUIRE(!range1.contains(0._f));
    REQUIRE(range1.contains(1._f));
    REQUIRE(range1.contains(2._f));
    REQUIRE(range1.contains(LARGE));

    Range range2(-INFTY, 1._f);
    REQUIRE(range2.contains(-1._f));
    REQUIRE(range2.contains(-LARGE));
    REQUIRE(range2.contains(1._f));
    REQUIRE(!range2.contains(2._f));
    REQUIRE(!range2.contains(LARGE));

    Range range3(-INFTY, INFTY);
    REQUIRE(range3.contains(-LARGE));
    REQUIRE(range3.contains(-1._f));
    REQUIRE(range3.contains(0._f));
    REQUIRE(range3.contains(1._f));
    REQUIRE(range3.contains(LARGE));
}

TEST_CASE("Range size", "[range]") {
    Range range1(0._f, 5._f);
    Range range2(-INFTY, 3._f);
    Range range3(1._f, INFTY);
    Range range4(-INFTY, INFTY);
    REQUIRE(range1.size() == 5._f);
    REQUIRE(range2.size() > LARGE);
    REQUIRE(range3.size() > LARGE);
    REQUIRE(range4.size() > LARGE);
}

TEST_CASE("Range comparison", "[range]") {
    Range range1(0._f, 2._f);
    Range range2(-INFTY, 3._f);
    Range range3(1._f, INFTY);
    Range range4(-INFTY, INFTY);

    REQUIRE(range1 != range2);
    REQUIRE(range1 != range3);
    REQUIRE(range1 != range4);
    REQUIRE(range2 != range3);
    REQUIRE(range2 != range4);
    REQUIRE(range3 != range4);

    REQUIRE(range1 == Range(0._f, 2._f));
    REQUIRE(range2 == Range(-INFTY, 3._f));
    REQUIRE(range3 == Range(1._f, INFTY));
    REQUIRE(range4 == Range(-INFTY, INFTY));
}

/*TEST_CASE("Range operators", "[range]") {
    Range range1(4._f, 8._f);
    Range range2(3._f, 4._f);
    Range range3(-INFTY, 5._f);
    Range range4(2._f, INFTY);
    Range range5(-INFTY, INFTY);

    REQUIRE(range1 * range2 == Range(12._f, 32._f));
    REQUIRE(range1 * range3 == Range(-INFTY, 40._f));
    REQUIRE(range2 * range4 == Range(6._f, INFTY));
    REQUIRE(range2 * range5 == Range(-INFTY, INFTY));
}*/

TEST_CASE("Range loop", "[range]") {
    Range range(0._f, 5._f);
    auto adapter = rangeAdapter(range, 1._f);
    REQUIRE(*(adapter.begin()) == 0._f);
    REQUIRE(*(adapter.end()) == 5._f);

    Float array[] = { 0._f, 1._f, 2._f, 3._f, 4._f };

    int idx = 0;
    for (Float& i : adapter) {
        REQUIRE(i == array[idx++]);
    }

    Float step = 0.5f;
    Range range2(0._f, 20._f);
    auto adapter2 = rangeAdapter(range2, step);
    REQUIRE(*(adapter2.begin()) == 0._f);
    REQUIRE(*(adapter2.end()) == 20._f);

    Float array2[] = { 0._f, 1._f, 3._f, 7._f, 15._f };
    idx = 0;
    for (Float& i : adapter2) {
        REQUIRE(i == array2[idx++]);
        step *= 2._f;
    }
}
