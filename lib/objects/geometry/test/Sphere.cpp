#include "objects/geometry/Sphere.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Sphere intersection", "[sphere]") {
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
