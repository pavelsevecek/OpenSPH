#include "geometry/Vector.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Vector construction", "[vector]") {
    // default construction
    REQUIRE_NOTHROW(Vector v1);

    // construct from single value
    Vector v2(5._f);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(v2[i] == 5._f);
    }

    // copy construct
    Vector v3(v2);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(v3[i] == 5._f);
    }

    // "move" construct
    Vector v4(Vector(3._f));
    for (int i = 0; i < 3; ++i) {
        REQUIRE(v4[i] == 3._f);
    }
}

TEST_CASE("Vector binary operators", "[vector]") {
    Vector v3(4._f, 6._f, -12._f);
    Vector v4(2._f, -3._f, -4._f);
    REQUIRE(v3 + v4 == Vector(6._f, 3._f, -16._f));
    REQUIRE(v3 - v4 == Vector(2._f, 9._f, -8._f));
    REQUIRE(v3 * v4 == Vector(8._f, -18._f, 48._f));
    REQUIRE(v3 / v4 == Vector(2._f, -2._f, 3._f));
    REQUIRE(v3 * 2._f == Vector(8._f, 12._f, -24._f));
    REQUIRE(2._f * v3 == Vector(8._f, 12._f, -24._f));
    REQUIRE(v3 / 2._f == Vector(2._f, 3._f, -6._f));
    REQUIRE(v3 * 2._f == Vector(8._f, 12._f, -24._f));
    REQUIRE(2._f * v3 == Vector(8._f, 12._f, -24._f));
    REQUIRE(v3 / 2._f == Vector(2._f, 3._f, -6._f));
}

TEST_CASE("Vector unary operators", "[vector]") {
    Vector v1(3._f, -4._f, 1._f);
    Vector v2(1._f, 2._f, 3._f);
    v1 += v2;
    REQUIRE(v1 == Vector(4._f, -2._f, 4._f));
    REQUIRE(v2 == Vector(1._f, 2._f, 3._f)); // unchanged
    v2 -= v1;
    REQUIRE(v2 == Vector(-3._f, 4._f, -1._f));
    v1 *= 2;
    REQUIRE(v1 == Vector(8._f, -4._f, 8._f));
    v1 /= 2;
    REQUIRE(v1 == Vector(4._f, -2._f, 4._f));

    REQUIRE(-v2 == Vector(3._f, -4, 1._f));
}

TEST_CASE("Vector comparisons 1", "[vector]") {
    Vector v(6._f, 3._f, 2._f);
    REQUIRE(v == v);
    REQUIRE(v == Vector(6._f, 3._f, 2._f));
    REQUIRE(Vector(6._f, 3._f, 2._f) == v);
    REQUIRE(v != Vector(5._f, 3._f, 2._f));
    REQUIRE(v != Vector(6._f, 4._f, 2._f));
    REQUIRE(v != Vector(6._f, 3._f, 1._f));
}

TEST_CASE("Vector comparisons 2", "[vectors]") {
    // dummy components should not influence equality
    const Vector v1(1.f, 1.f, 3.f, 5.f);
    const Vector v2(1.f, 2.f, 4.f, 0.f);
    REQUIRE(v1 != v2);

    const Vector v3(1.f, 1.f, 3.f, 5.f);
    const Vector v4(1.f, 1.f, 3.f, 0.f);
    REQUIRE(v3 == v4);
}

TEST_CASE("Vector length", "[vector]") {
    Vector v1(3._f, 4._f, 12._f);
    REQUIRE(getSqrLength(v1) == 169._f);
    REQUIRE(getLength(v1) == 13._f);
    Vector v2(1._f);
    REQUIRE(getLength(v2) == Math::sqrt(3._f));
}

TEST_CASE("Vector products", "[vector]") {
    // dot product
    Vector v1(1._f, 2._f, 3._f);
    Vector v2(4._f, -5._f, 6._f);
    REQUIRE(dot(v1, v2) == 12._f);
    REQUIRE(dot(v2, v1) == 12._f);

    // cross product
    Vector expected(27._f, 6._f, -13._f);
    REQUIRE(cross(v1, v2) == expected);
    REQUIRE(cross(v2, v1) == -expected);
}

TEST_CASE("Vector utilities", "[vector]") {
    // spherical coordinates
    Vector v = spherical(Math::sqrt(2._f), Math::PI / 2._f, Math::PI / 4._f);
    REQUIRE(Math::almostEqual(v, Vector(1._f, 1._f, 0._f), EPS));
}

TEST_CASE("Vector inequalities", "[vectors]") {
    const int nRounds = 10;
    for (int i = 0; i < nRounds; ++i) {
        // normalization
        const Vector v1 = randomVector();
        REQUIRE(Math::abs(getLength(getNormalized(v1)) - 1._f) <= EPS);

        // triangle inequality
        const Vector v2 = randomVector();
        REQUIRE(getLength(v1 + v2) <= getLength(v1) + getLength(v2));

        // Cauchy-Schwarz inequality
        REQUIRE(Math::abs(dot(v1, v2)) <= getLength(v1) * getLength(v2));
    }
}

TEST_CASE("Vector product", "[vectors]") {
    // for d=3 only
    const int nRounds = 10;
    for (int i          = 0; i < nRounds; ++i) {
        const Vector v1 = randomVector();
        const Vector v2 = randomVector();
        // cross product is perpendicular to both vectors
        const Vector c   = cross(v1, v2);
        const float dot1 = dot(c, v1);
        const float dot2 = dot(c, v2);
        REQUIRE(Math::abs(dot1) <= EPS);
        REQUIRE(Math::abs(dot2) <= EPS);
    }
}

TEST_CASE("Component-wise min and max", "[vectors]") {
    Vector v1(6._f, -7._f, 8._f);
    Vector v2(-1._f, 3._f, 5._f);
    REQUIRE(Math::max(v1, v2) == Vector(6._f, 3._f, 8._f));
    REQUIRE(Math::min(v1, v2) == Vector(-1._f, -7._f, 5._f));
}

