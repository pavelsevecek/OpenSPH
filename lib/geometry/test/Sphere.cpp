#include "geometry/Sphere.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Sphere intersection", "[sphere]") {
    Sphere sphere(Vector(0._f), 1._f);
    // box enclosing the sphere
    REQUIRE(sphere.intersectsBox(Box(Vector(-2), Vector(2))) == IntersectResult::BOX_CONTAINS_SPHERE);
    // box inside the sphere
    REQUIRE(sphere.intersectsBox(Box(Vector(-0.5), Vector(0.5))) == IntersectResult::SPHERE_CONTAINS_BOX);
    // sphere center inside the box, sphere intersects the box
    REQUIRE(sphere.intersectsBox(Box(Vector(-0.9), Vector(0.9))) == IntersectResult::INTERESECTION);
    // box outside the box
    REQUIRE(sphere.intersectsBox(Box(Vector(2), Vector(3))) == IntersectResult::NO_INTERSECTION);
    // intersection
    REQUIRE(sphere.intersectsBox(Box(Vector(0.5, 0, 0), Vector(2, 1, 1))) == IntersectResult::INTERESECTION);
}
