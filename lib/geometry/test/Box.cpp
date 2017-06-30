#include "geometry/Box.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Box default construction", "[box]") {
    Box box;
    REQUIRE_FALSE(box.contains(Vector(0._f)));
    REQUIRE_FALSE(box.contains(Vector(INFTY)));
    REQUIRE_FALSE(box.contains(Vector(-INFTY)));
    REQUIRE_FALSE(box.contains(Vector(1._f, 0._f, -1._f)));

    const Vector v(5._f, -7._f, 3._f);
    box.extend(v);
    REQUIRE(box.lower() == v);
    REQUIRE(box.upper() == v);
    REQUIRE(box.center() == v);
    REQUIRE(box.size() == Vector(0._f));
    REQUIRE(box.volume() == 0._f);
    REQUIRE(box.contains(v));
    REQUIRE_FALSE(box.contains(Vector(0._f)));
    REQUIRE_FALSE(box.contains(Vector(INFTY)));
    REQUIRE_FALSE(box.contains(Vector(5._f, -7._f - EPS, 3._f)));
}

TEST_CASE("Box bound construction", "[box]") {
    Box box1(Vector(1._f, 0._f, 2._f), Vector(3._f, 0._f, 5._f));
    REQUIRE(box1.lower() == Vector(1._f, 0._f, 2._f));
    REQUIRE(box1.upper() == Vector(3._f, 0._f, 5._f));
    REQUIRE(box1.center() == Vector(2._f, 0._f, 3.5_f));
    REQUIRE(box1.size() == Vector(2._f, 0._f, 3._f));
    REQUIRE(box1.volume() == 0._f);

    REQUIRE(box1.contains(Vector(2._f, 0._f, 3._f)));
    REQUIRE(box1.contains(Vector(1._f, 0._f, 5._f)));
    REQUIRE_FALSE(box1.contains(Vector(1._f - EPS, 0._f, 5._f)));

    Box box2(Vector(-1._f), Vector(4._f));
    REQUIRE(box2.lower() == Vector(-1._f));
    REQUIRE(box2.upper() == Vector(4._f));
    REQUIRE(box2.center() == Vector(1.5_f));
    REQUIRE(box2.size() == Vector(5._f));
    REQUIRE(box2.volume() == 125._f);
    REQUIRE(box2.contains(Vector(0._f)));
    REQUIRE(box2.contains(Vector(-1._f)));
    REQUIRE(box2.contains(Vector(4._f)));
    REQUIRE(box2.contains(Vector(-1._f, -1._f, 4._f)));
    REQUIRE_FALSE(box2.contains(Vector(-1._f - EPS)));
    REQUIRE_FALSE(box2.contains(Vector(0._f, 0._f, -2._f)));
    REQUIRE_FALSE(box2.contains(Vector(0._f, 4.5_f, 0._f)));
}

TEST_CASE("Box extend", "[box]") {
    Box box(Vector(0._f), Vector(0._f));
    box.extend(Vector(-1.f, 0._f, 0._f));
    REQUIRE(box.lower() == Vector(-1._f, 0._f, 0._f));
    REQUIRE(box.upper() == Vector(0._f, 0._f, 0._f));
    REQUIRE(box.center() == Vector(-0.5_f, 0._f, 0._f));
    box.extend(Vector(0._f, 2._f, 0._f));
    REQUIRE(box.lower() == Vector(-1._f, 0._f, 0._f));
    REQUIRE(box.upper() == Vector(0._f, 2._f, 0._f));
    REQUIRE(box.center() == Vector(-0.5_f, 1._f, 0._f));
    box.extend(Vector(3._f, -4._f, 6._f));
    REQUIRE(box.lower() == Vector(-1._f, -4._f, 0._f));
    REQUIRE(box.upper() == Vector(3._f, 2._f, 6._f));
    REQUIRE(box.center() == Vector(1._f, -1._f, 3._f));
}

TEST_CASE("Box clamp", "[box]") {
    Box box(Vector(1._f), Vector(2._f, 3._f, 4._f));
    REQUIRE(box.clamp(Vector(1._f)) == Vector(1._f));
    REQUIRE(box.clamp(Vector(0._f)) == Vector(1._f));
    REQUIRE(box.clamp(Vector(3._f, 0._f, -1._f)) == Vector(2._f, 1._f, 1._f));
    REQUIRE(box.clamp(Vector(-1._f, 4._f, 5._f)) == Vector(1._f, 3._f, 4._f));
    REQUIRE(box.clamp(Vector(INFTY)) == box.upper());
    REQUIRE(box.clamp(Vector(-INFTY)) == box.lower());
}

TEST_CASE("Box iterate", "[box]") {
    Box box(Vector(0._f), Vector(2._f, 3._f, 4._f));
    Array<Vector> vs;
    box.iterate(Vector(0.5_f), [&](const Vector& v) { vs.push(v); });
    REQUIRE(vs.size() == 5 * 7 * 9);
    REQUIRE(vs[0] == Vector(0._f));
    REQUIRE(vs[vs.size() - 1] == Vector(2._f, 3._f, 4._f));
    REQUIRE(vs[(vs.size() - 1) >> 1] == Vector(1._f, 1.5_f, 2._f));
}

TEST_CASE("Box iterateWithIndices", "[box]") {
    Box box(Vector(0._f), Vector(2._f, 3._f, 4._f));
    Array<Vector> vs;
    Array<Indices> idxs;
    box.iterateWithIndices(Vector(0.5_f), [&](const Indices& i, const Vector& v) {
        idxs.push(i);
        vs.push(v);
    });
    REQUIRE(vs.size() == 5 * 7 * 9);
    REQUIRE(idxs.size() == 5 * 7 * 9);
    REQUIRE(vs[0] == Vector(0._f));
    REQUIRE(vs[vs.size() - 1] == Vector(2._f, 3._f, 4._f));
    REQUIRE(vs[(vs.size() - 1) >> 1] == Vector(1._f, 1.5_f, 2._f));
    REQUIRE(all(idxs[0] == Indices(0)));
    REQUIRE(all(idxs[idxs.size() - 1] == Indices(4, 6, 8)));
    REQUIRE(all(idxs[(idxs.size() - 1) >> 1] == Indices(2, 3, 4)));
}

TEST_CASE("Box split", "[box]") {
    Box box(Vector(0._f), Vector(2._f, 4._f, 6._f));
    Box b1, b2;
    tie(b1, b2) = box.split(X, 0.5_f);
    REQUIRE(b1 == Box(Vector(0._f), Vector(0.5_f, 4._f, 6._f)));
    REQUIRE(b2 == Box(Vector(0.5_f), Vector(2._f, 4._f, 6._f)));

    tie(b1, b2) = box.split(Z, 4._f);
    REQUIRE(b1 == Box(Vector(0._f), Vector(2._f, 4._f, 4._f)));
    REQUIRE(b2 == Box(Vector(0._f, 0._f, 4._f), Vector(2._f, 4._f, 6._f)));
}
