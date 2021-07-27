#include "math/AffineMatrix.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Matrix rows&columns", "[affinematrix]") {
    AffineMatrix m(Vector(1, 2, 3, 4), Vector(5, 6, 7, 8), Vector(9, 10, 11, 12));

    REQUIRE(m.row(0) == Vector(1, 2, 3));
    REQUIRE(m.row(1) == Vector(5, 6, 7));
    REQUIRE(m.row(2) == Vector(9, 10, 11));
    REQUIRE_SPH_ASSERT(m.row(3));

    REQUIRE(m.column(0) == Vector(1, 5, 9));
    REQUIRE(m.column(1) == Vector(2, 6, 10));
    REQUIRE(m.column(2) == Vector(3, 7, 11));
    REQUIRE(m.column(3) == Vector(4, 8, 12));
    REQUIRE_SPH_ASSERT(m.column(4));

    REQUIRE(m.translation() == Vector(4, 8, 12));
}

TEST_CASE("Matrix elements", "[affinematrix]") {
    AffineMatrix m(Vector(1, 2, 3, 4), Vector(5, 6, 7, 8), Vector(9, 10, 11, 12));
    REQUIRE(m(0, 0) == 1);
    REQUIRE(m(0, 1) == 2);
    REQUIRE(m(0, 2) == 3);
    REQUIRE(m(0, 3) == 4);
    REQUIRE(m(1, 0) == 5);
    REQUIRE(m(1, 1) == 6);
    REQUIRE(m(1, 2) == 7);
    REQUIRE(m(1, 3) == 8);
    REQUIRE(m(2, 0) == 9);
    REQUIRE(m(2, 1) == 10);
    REQUIRE(m(2, 2) == 11);
    REQUIRE(m(2, 3) == 12);

    REQUIRE_SPH_ASSERT(m(3, 0));
    REQUIRE_SPH_ASSERT(m(0, 4));
}

TEST_CASE("Matrix operations", "[affinematrix]") {
    AffineMatrix m1(Vector(1, 2, -1, 0), Vector(0, 2, 3, -2), Vector(3, 3, -1, 4));
    REQUIRE(m1 * 2 == AffineMatrix(Vector(2, 4, -2, 0), Vector(0, 4, 6, -4), Vector(6, 6, -2, 8)));
    REQUIRE_FALSE(m1 * 2 == AffineMatrix(Vector(2, 4, -2, 0), Vector(0, 4, 6, -4), Vector(6, 6, -2, 7)));
    REQUIRE(-3 * m1 == m1 * -3);

    AffineMatrix m2(Vector(3, 2, 0, 1), Vector(2, -4, 1, -1), Vector(2, 1, 2, -3));
    REQUIRE(m1 + m2 == AffineMatrix(Vector(4, 4, -1, 1), Vector(2, -2, 4, -3), Vector(5, 4, 1, 1)));
    REQUIRE_FALSE(m1 + m2 == AffineMatrix(Vector(1, 4, -1, 1), Vector(2, -2, 4, -3), Vector(5, 4, 1, 1)));
    REQUIRE_FALSE(m1 + m2 == AffineMatrix(Vector(4, 4, -1, -1), Vector(2, -2, 4, -3), Vector(5, 4, 1, 1)));

    /// \todo other operations
}

TEST_CASE("Matrix apply", "[affinematrix]") {
    AffineMatrix tr = AffineMatrix::identity().translate(Vector(1, -3, 2));
    const Vector v(5, -2, -1);
    REQUIRE(tr * v == Vector(6, -5, 1));

    AffineMatrix m{ { 2, 0.5, -1, -2 }, { 0, 1, -1, 0.5 }, { 3, -2, 1, 0 } };
    REQUIRE(m * v == Vector(8, -0.5, 18));
}

TEST_CASE("Matrix multiplication", "[affinematrix]") {
    const Vector v1(1, -3, 2);
    AffineMatrix tr1 = AffineMatrix::identity().translate(v1);
    const Vector v2(-2, 4, 5);
    AffineMatrix tr2 = AffineMatrix::identity().translate(v2);
    AffineMatrix res = tr1 * tr2;
    REQUIRE(res.translation() == v1 + v2);
    REQUIRE(res.translation() == v1 + v2);
    REQUIRE(res.removeTranslation() == AffineMatrix::identity());

    AffineMatrix rot = AffineMatrix::rotateZ(PI / 2._f);
    res = tr1 * rot;
    REQUIRE(res == rot.translate(v1));

    rot = AffineMatrix::rotateZ(PI / 2._f);
    res = rot * tr1;
    REQUIRE(res == rot.translate(rot * v1));
}

TEST_CASE("Matrix transpose", "[affinematrix]") {
    AffineMatrix m(Vector(1, 2, 3), Vector(4, 5, 6), Vector(7, 8, 9));
    AffineMatrix mt = m.transpose();
    REQUIRE(m.row(0) == mt.column(0));
    REQUIRE(m.row(1) == mt.column(1));
    REQUIRE(m.row(2) == mt.column(2));
    REQUIRE(mt.row(0) == m.column(0));
    REQUIRE(mt.row(1) == m.column(1));
    REQUIRE(mt.row(2) == m.column(2));
}

TEST_CASE("Matrix inverse", "[affinematrix]") {
    AffineMatrix tr = AffineMatrix::identity();
    tr.translate(Vector(4._f, 2._f, 1._f));
    REQUIRE(tr.inverse() == AffineMatrix::identity().translate(Vector(-4._f, -2._f, -1._f)));

    AffineMatrix rotX = AffineMatrix::rotateX(0.2_f);
    REQUIRE(rotX.inverse() == approx(rotX.transpose()));

    AffineMatrix m{ { 2, 0.5, -1, -2 }, { 0, 1, -1, 0.5 }, { 3, -2, 1, 0 } };
    AffineMatrix m_inv{ { 2., -3., -1., 5.5 }, { 6., -10., -4., 17. }, { 6., -11., -4., 17.5 } };
    REQUIRE(m.inverse() == m_inv);


    REQUIRE_SPH_ASSERT(AffineMatrix::null().inverse());
}

TEST_CASE("Matrix tryInverse", "[affinematrix]") {
    REQUIRE_FALSE(AffineMatrix::null().tryInverse());
    AffineMatrix id = AffineMatrix::identity();
    Optional<AffineMatrix> invId = id.tryInverse();
    REQUIRE(invId);
    REQUIRE(invId.value() == approx(id));
}

TEST_CASE("Matrix isOrthogonal", "[affinematrix]") {
    REQUIRE(AffineMatrix::identity().isOrthogonal());
    REQUIRE_FALSE(AffineMatrix::null().isOrthogonal());
    REQUIRE(AffineMatrix::rotateX(0.2_f).isOrthogonal());
    REQUIRE_FALSE(AffineMatrix::scale(Vector(2._f, 1._f, 0.5_f)).isOrthogonal());
}

TEST_CASE("Matrix scaling", "[affinematrix]") {
    AffineMatrix s = AffineMatrix::scale(Vector(2._f, 3._f, -1._f));
    REQUIRE(s * Vector(2._f, 3._f, 4._f) == Vector(4._f, 9._f, -4._f));
}

TEST_CASE("Matrix rotateX", "[affinematrix]") {
    AffineMatrix rot = AffineMatrix::rotateX(PI / 2._f);
    REQUIRE(rot * Vector(2._f, 3._f, 4._f) == approx(Vector(2._f, -4._f, 3._f)));
}

TEST_CASE("Matrix rotateY", "[affinematrix]") {
    AffineMatrix rot = AffineMatrix::rotateY(PI / 2._f);
    REQUIRE(rot * Vector(2._f, 3._f, 4._f) == approx(Vector(4._f, 3._f, -2._f)));
}

TEST_CASE("Matrix rotateZ", "[affinematrix]") {
    AffineMatrix rot = AffineMatrix::rotateZ(PI / 2._f);
    REQUIRE(rot * Vector(2._f, 3._f, 4._f) == approx(Vector(-3._f, 2._f, 4._f)));
}

TEST_CASE("Matrix rotateAxis", "[affinematrix]") {
    AffineMatrix rotX = AffineMatrix::rotateX(0.7_f);
    AffineMatrix rotY = AffineMatrix::rotateY(-1.9_f);
    AffineMatrix rotZ = AffineMatrix::rotateZ(0.5_f);

    REQUIRE(rotX == AffineMatrix::rotateAxis(Vector(1._f, 0._f, 0._f), 0.7_f));
    REQUIRE(rotY == AffineMatrix::rotateAxis(Vector(0._f, 1._f, 0._f), -1.9_f));
    REQUIRE(rotZ == AffineMatrix::rotateAxis(Vector(0._f, 0._f, 1._f), 0.5_f));
}
