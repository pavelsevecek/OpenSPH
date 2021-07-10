#include "objects/wrappers/Lut.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Lut evaluate", "[lut]") {
    Lut<Float> lut(Interval(0._f, 2._f * PI), 10000, Sph::sin<Float>);

    REQUIRE(lut(10._f) == approx(0._f));
    REQUIRE(lut(-10._f) == approx(0._f));

    for (Float x = 0._f; x <= 2._f * PI; x += 0.5_f) {
        REQUIRE(lut(x) == approx(sin(x), 1.e-5_f));
    }
}

TEST_CASE("Lut iterate", "[lut]") {
    Array<Float> data({ 1._f, 4._f, 9._f, 16._f });
    Lut<Float> lut(Interval(1._f, 4._f), std::move(data));

    Size index = 1;
    for (auto p : lut) {
        REQUIRE(p.x == index);
        REQUIRE(p.y == sqr(index));
        index++;
    }
}

using Value = LutIterator<Float>::Value;

namespace Sph {
bool almostEqual(const Value& v1, const Value& v2, const Float eps) {
    return almostEqual(v1.x, v2.x, eps) && almostEqual(v1.y, v2.y, eps);
}
} // namespace Sph

bool compare(const Value& p1, const Value& p2) {
    return p1 == approx(p2, 1.e-3_f);
}

TEST_CASE("Lut differentiate", "[lut]") {
    Lut<Float> lut(Interval(0._f, 2._f * PI), 10000, Sph::sin<Float>);
    Lut<Float> expected(Interval(0._f, 2._f * PI), 10000, Sph::cos<Float>);
    Lut<Float> actual = lut.derivative();

    const bool match = std::equal(expected.begin(), expected.end(), actual.begin(), compare);
    REQUIRE(match);
}

TEST_CASE("Lut integrate", "[lut]") {
    Lut<Float> lut(Interval(0._f, 2._f * PI), 10000, Sph::cos<Float>);
    Lut<Float> expected(Interval(0._f, 2._f * PI), 10000, Sph::sin<Float>);
    Lut<Float> actual = lut.integral(0._f);

    const bool match = std::equal(expected.begin(), expected.end(), actual.begin(), compare);
    REQUIRE(match);
}
