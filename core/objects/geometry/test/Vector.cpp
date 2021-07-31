#include "objects/geometry/Vector.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Vector construction", "[vector]") {
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

    Vector v5(5._f, 4._f, 3._f, 2._f);
    REQUIRE(v5[0] == 5._f);
    REQUIRE(v5[1] == 4._f);
    REQUIRE(v5[2] == 3._f);
    REQUIRE(v5[3] == 2._f);
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
    REQUIRE(getLength(v2) == Sph::sqrt(3._f));
}

TEST_CASE("Vector normalization", "[vector]") {
    Vector v1(3._f, 4._f, 5._f);
    const Float length = getLength(v1);
    Vector nv1 = getNormalized(v1);
    REQUIRE(nv1[0] == 3._f / length);
    REQUIRE(nv1[1] == 4._f / length);
    REQUIRE(nv1[2] == 5._f / length);
    REQUIRE(nv1 == approx(v1 / length));

    Float l;
    Vector nv2;
    tieToTuple(nv2, l) = getNormalizedWithLength(v1);
    REQUIRE(l == approx(length));
    REQUIRE(nv2 == approx(nv1));
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

    const int nRounds = 10;
    for (int i = 0; i < nRounds; ++i) {
        const Vector v1 = randomVector();
        const Vector v2 = randomVector();
        // cross product is perpendicular to both vectors
        const Vector c = cross(v1, v2);
        const Float dot1 = dot(c, v1);
        const Float dot2 = dot(c, v2);
        REQUIRE(abs(dot1) <= EPS);
        REQUIRE(abs(dot2) <= EPS);
    }
}

TEST_CASE("Vector utilities", "[vector]") {
    // spherical coordinates
    Vector v = sphericalToCartesian(Sph::sqrt(2._f), PI / 2._f, PI / 4._f);
    REQUIRE(v == approx(Vector(1._f, 1._f, 0._f)));

    SphericalCoords spherical = cartensianToSpherical(v);
    REQUIRE(spherical.r == approx(sqrt(2._f)));
    REQUIRE(spherical.theta == approx(PI / 2._f));
    REQUIRE(spherical.phi == approx(PI / 4._f));
}

TEST_CASE("Vector inequalities", "[vectors]") {
    const int nRounds = 10;
    for (int i = 0; i < nRounds; ++i) {
        // normalization
        const Vector v1 = randomVector();
        REQUIRE(abs(getLength(getNormalized(v1)) - 1._f) <= EPS);

        // triangle inequality
        const Vector v2 = randomVector();
        REQUIRE(getLength(v1 + v2) <= getLength(v1) + getLength(v2));

        // Cauchy-Schwarz inequality
        REQUIRE(abs(dot(v1, v2)) <= getLength(v1) * getLength(v2));
    }
}

TEST_CASE("Vector Component-wise min and max", "[vectors]") {
    Vector v1(6._f, -7._f, 8._f);
    Vector v2(-1._f, 3._f, 5._f);
    REQUIRE(max(v1, v2) == Vector(6._f, 3._f, 8._f));
    REQUIRE(min(v1, v2) == Vector(-1._f, -7._f, 5._f));
}

TEST_CASE("Vector MinElement", "[vector]") {
    REQUIRE(minElement(Vector(-1._f, 5._f, 2._f)) == -1._f);
    REQUIRE(minElement(Vector(5._f, 5._f, 2._f)) == 2._f);
    REQUIRE(minElement(Vector(-1._f, -5._f, 3._f)) == -5._f);
}

TEST_CASE("Vector abs", "[vector]") {
    REQUIRE(abs(Vector(-1._f, 0._f, 1._f)) == Vector(1._f, 0._f, 1._f));
    REQUIRE(abs(Vector(-1._f, -2._f, -5._f)) == Vector(1._f, 2._f, 5._f));
    REQUIRE(abs(Vector(0._f)) == Vector(0._f));
    REQUIRE(abs(Vector(5._f, 5._f, -1._f)) == Vector(5._f, 5._f, 1._f));
}

TEST_CASE("Vector cast", "[vector]") {
    BasicVector<float> vf(1.f, 2.f, 3.f, 4.f);
    BasicVector<double> dv = vectorCast<double>(vf);
    REQUIRE(dv == BasicVector<double>(1., 2., 3., 4.));

    BasicVector<float> vf2 = vectorCast<float>(dv);
    REQUIRE(vf2 == BasicVector<float>(1.f, 2.f, 3.f, 4.f));

    BasicVector<float> vf3 = vectorCast<float>(vf2); // casting on the same precision
    REQUIRE(vf3 == BasicVector<float>(1.f, 2.f, 3.f, 4.f));
}

TEST_CASE("Vector almostEqual", "[vector]") {
    REQUIRE(almostEqual(Vector(1._f, 2._f, 3._f), Vector(1._f, 2._f, 3._f)));
    REQUIRE_FALSE(almostEqual(Vector(1._f, 2._f, 3._f), Vector(1._f, -2._f, 3._f)));
    REQUIRE_FALSE(almostEqual(Vector(1._f, 2._f, 3._f), Vector(1._f, 2._f, 2.9_f)));
    REQUIRE(almostEqual(Vector(1._f, 2._f, 3._f), Vector(1._f, 2._f, 2.9_f), 0.1_f));

    REQUIRE(almostEqual(Vector(1.e10_f), Vector(1.1e10_f), 0.1_f));
    REQUIRE_FALSE(almostEqual(Vector(1.e10_f), Vector(1.1e10_f), 0.01_f));
    REQUIRE(almostEqual(Vector(1.e12_f, -2.e12_f, 0.5_f), Vector(1.e12_f, -2.e12_f, 10._f), 1.e-6_f));
    REQUIRE_FALSE(almostEqual(Vector(1.e12_f, -2.e12_f, 0.5_f), Vector(1.e12_f, -2.e12_f, 10._f), 1e-12_f));

    REQUIRE(almostEqual(Vector(1.e-10_f), Vector(1.1e-10_f), 1.e-6_f));
    REQUIRE_FALSE(almostEqual(Vector(1.e-10_f), Vector(1.1e-10_f), 1.e-15_f));
    REQUIRE(almostEqual(Vector(1.e-12_f, -2.e-12_f, 0._f), Vector(1.e-12_f, 1.e-18_f - 2.e-12_f, 0._f)));
}

TEST_CASE("Vector lexicographicalLess", "[vector]") {
    REQUIRE(lexicographicalLess(Vector(5._f, 3._f, 1._f), Vector(2._f, 1._f, 2._f)));
    REQUIRE_FALSE(lexicographicalLess(Vector(5._f, 3._f, 1._f), Vector(2._f, 1._f, 0.5_f)));
    REQUIRE(lexicographicalLess(Vector(5._f, 0._f, 1._f), Vector(2._f, 1._f, 1._f)));
    REQUIRE_FALSE(lexicographicalLess(Vector(5._f, 3._f, 1._f), Vector(2._f, 1._f, 1._f)));
    REQUIRE(lexicographicalLess(Vector(1._f, 3._f, 1._f), Vector(2._f, 3._f, 1._f)));
    REQUIRE_FALSE(lexicographicalLess(Vector(5._f, 3._f, 1._f), Vector(2._f, 3._f, 1._f)));
}

TEST_CASE("Vector less", "[vector]") {
    Vector v1(2._f, 3._f, 6._f);
    Vector v2(3._f, 3._f, 3._f);
    REQUIRE(less(v1, v2) == Vector(1._f, 0._f, 0.f));

    Vector v3(2._f, 5._f, -1._f, 1._f);
    Vector v4(3._f, 6._f, -2._f, -3._f);
    REQUIRE(less(v3, v4) == Vector(1._f, 1._f, 0._f, 0._f));
}

TEST_CASE("Vector unit", "[vector]") {
    REQUIRE(Vector::unit(0) == Vector(1, 0, 0));
    REQUIRE(Vector::unit(1) == Vector(0, 1, 0));
    REQUIRE(Vector::unit(2) == Vector(0, 0, 1));
    REQUIRE_SPH_ASSERT(Vector::unit(3));
}

/// \todo also test tensors
/*TEST_CASE("Vector clampWithDerivative", "[vector]") {
    Vector v(-1._f, 3._f, 6._f);
    Vector dv(20._f, 21._f, 22._f);
    Range r(0._f, 4._f);
    tie(v, dv) = clampWithDerivative(v, dv, r);
    REQUIRE(v == Vector(0._f, 3._f, 4._f));
    REQUIRE(dv == Vector(0._f, 21._f, 0._f));
}
*/
