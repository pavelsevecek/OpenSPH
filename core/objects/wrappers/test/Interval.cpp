#include "objects/wrappers/Interval.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Interval contains", "[interval]") {
    Interval range;
    REQUIRE(range.empty());
    REQUIRE(!range.contains(0._f));
    REQUIRE(!range.contains(LARGE));
    REQUIRE(!range.contains(-LARGE));

    range.extend(0._f);
    REQUIRE(!range.empty());
    REQUIRE(range.contains(0._f));
    REQUIRE(!range.contains(1._f));
    REQUIRE(!range.contains(-1._f));

    range.extend(1._f);
    REQUIRE(range.contains(0._f));
    REQUIRE(range.contains(1._f));
    REQUIRE(!range.contains(2._f));
    REQUIRE(!range.contains(-1._f));
}

TEST_CASE("Interval intersection", "[interval]") {
    Interval range(1._f, 5._f);
    REQUIRE(range.intersection(Interval(2._f, 3._f)) == Interval(2._f, 3._f));
    REQUIRE(range.intersection(Interval(4._f, 7._f)) == Interval(4._f, 5._f));
    REQUIRE(range.intersection(Interval(0._f, 6._f)) == Interval(1._f, 5._f));
    REQUIRE(range.intersection(Interval(-1._f, 2._f)) == Interval(1._f, 2._f));
    REQUIRE(range.intersection(Interval(-1._f, 0._f)).empty());
    REQUIRE(range.intersection(Interval(6._f, 7._f)).empty());
}

TEST_CASE("Interval clamp", "[interval]") {
    Interval range(0._f, 1._f);
    REQUIRE(range.clamp(2._f) == 1._f);
    REQUIRE(range.clamp(1._f) == 1._f);
    REQUIRE(range.clamp(0.5_f) == 0.5_f);
    REQUIRE(range.clamp(0._f) == 0._f);
    REQUIRE(range.clamp(-0.5_f) == 0._f);
}

TEST_CASE("Interval extend", "[interval]") {
    Interval range;
    range.extend(Interval());
    REQUIRE(range.empty());

    range.extend(Interval(1._f, 1._f));
    REQUIRE(!range.empty());
    REQUIRE(range.size() == 0._f);
    REQUIRE(range.contains(1._f));

    range.extend(Interval(-2._f, -1._f));
    REQUIRE(range.size() == 3._f);
    REQUIRE(range == Interval(-2._f, 1._f));
}

TEST_CASE("Interval one sided", "[interval]") {
    Interval range1(1._f, INFTY);
    REQUIRE(!range1.contains(-LARGE));
    REQUIRE(!range1.contains(0._f));
    REQUIRE(range1.contains(1._f));
    REQUIRE(range1.contains(2._f));
    REQUIRE(range1.contains(LARGE));

    Interval range2(-INFTY, 1._f);
    REQUIRE(range2.contains(-1._f));
    REQUIRE(range2.contains(-LARGE));
    REQUIRE(range2.contains(1._f));
    REQUIRE(!range2.contains(2._f));
    REQUIRE(!range2.contains(LARGE));

    Interval range3(-INFTY, INFTY);
    REQUIRE(range3.contains(-LARGE));
    REQUIRE(range3.contains(-1._f));
    REQUIRE(range3.contains(0._f));
    REQUIRE(range3.contains(1._f));
    REQUIRE(range3.contains(LARGE));
}

TEST_CASE("Interval size", "[interval]") {
    Interval range1(0._f, 5._f);
    Interval range2(-INFTY, 3._f);
    Interval range3(1._f, INFTY);
    Interval range4(-INFTY, INFTY);
    REQUIRE(range1.size() == 5._f);
    REQUIRE(range2.size() > LARGE);
    REQUIRE(range3.size() > LARGE);
    REQUIRE(range4.size() > LARGE);
}

TEST_CASE("Interval comparison", "[interval]") {
    Interval range1(0._f, 2._f);
    Interval range2(-INFTY, 3._f);
    Interval range3(1._f, INFTY);
    Interval range4(-INFTY, INFTY);

    REQUIRE(range1 != range2);
    REQUIRE(range1 != range3);
    REQUIRE(range1 != range4);
    REQUIRE(range2 != range3);
    REQUIRE(range2 != range4);
    REQUIRE(range3 != range4);

    REQUIRE(range1 == Interval(0._f, 2._f));
    REQUIRE(range2 == Interval(-INFTY, 3._f));
    REQUIRE(range3 == Interval(1._f, INFTY));
    REQUIRE(range4 == Interval(-INFTY, INFTY));
}
