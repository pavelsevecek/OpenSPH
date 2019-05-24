#include "objects/geometry/Sphere.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Sphere constructor", "[sphere]") {
    REQUIRE_NOTHROW(Sphere(Vector(0._f), 1._f));
    REQUIRE_ASSERT(Sphere(Vector(0._f), -1._f));
}

TEST_CASE("Sphere contains", "[sphere]") {
    Sphere sphere(Vector(1._f, 0._f, 0._f), 2._f);
    REQUIRE(sphere.contains(Vector(1._f, 0._f, 0._f)));
    REQUIRE(sphere.contains(Vector(2.999_f, 0._f, 0._f)));
    REQUIRE(sphere.contains(Vector(1._f, 1.999_f, 0._f)));
    REQUIRE_FALSE(sphere.contains(Vector(-1.1_f, 0._f, 0._f)));
}

TEST_CASE("Sphere intersectsSphere", "[sphere]") {
    Sphere sphere(Vector(0._f), 1._f);
    REQUIRE(sphere.intersects(Sphere(Vector(0._f), 0.5_f)));
    REQUIRE(sphere.intersects(Sphere(Vector(0.5_f), 2._f)));
    REQUIRE(sphere.intersects(Sphere(Vector(1.5_f, 0._f, 0._f), 0.6_f)));

    REQUIRE_FALSE(sphere.intersects(Sphere(Vector(1.5_f, 0._f, 0._f), 0.45_f)));
    REQUIRE_FALSE(sphere.intersects(Sphere(Vector(0._f, 1._f + 2._f * EPS, 0._f), EPS)));
}

TEST_CASE("Sphere intersectsBox", "[sphere]") {
    Sphere sphere(Vector(0._f), 1._f);
    /*// box enclosing the sphere
    /// \todo how is this case classified? (currently not used by TreeWalk, but should be implemented
    nevertheless
    REQUIRE(sphere.intersectsBox(Box(Vector(-2), Vector(2))) == IntersectResult::BOX_ BOX_CONTAINS_SPHERE);*/
    // box inside the sphere
    REQUIRE(sphere.intersectsBox(Box(Vector(-0.5), Vector(0.5))) == IntersectResult::BOX_INSIDE_SPHERE);
    // sphere center inside the box, sphere intersects the box
    REQUIRE(sphere.intersectsBox(Box(Vector(-0.9), Vector(0.9))) == IntersectResult::INTERESECTION);
    // box outside the sphere
    REQUIRE(sphere.intersectsBox(Box(Vector(2), Vector(3))) == IntersectResult::BOX_OUTSIDE_SPHERE);
    // box inside the sphere, but the center of the sphere is not inside the box
    REQUIRE(sphere.intersectsBox(Box(Vector(0.4), Vector(0.5))) == IntersectResult::BOX_INSIDE_SPHERE);
    // intersection
    REQUIRE(sphere.intersectsBox(Box(Vector(0.5, 0, 0), Vector(2, 1, 1))) == IntersectResult::INTERESECTION);
}
