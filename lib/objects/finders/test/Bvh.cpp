#include "objects/finders/Bvh.h"
#include "catch.hpp"
#include "math/rng/VectorRng.h"

using namespace Sph;

TEST_CASE("Box intersect", "[bvh]") {
    Box box(Vector(0._f), Vector(1._f));

    Ray ray1(Vector(2._f, 0.5_f, 0.5_f), Vector(-1._f, 0._f, 0._f));
    Float t_min, t_max;
    REQUIRE(intersectBox(box, ray1, t_min, t_max));
    REQUIRE(t_min == 1._f);
    REQUIRE(t_max == 2._f);

    // same ray, different parametrization
    Ray ray2(Vector(2._f, 0.5_f, 0.5_f), Vector(-0.5_f, 0._f, 0._f));
    REQUIRE(intersectBox(box, ray2, t_min, t_max));
    REQUIRE(t_min == 2._f);
    REQUIRE(t_max == 4._f);

    Ray ray3(Vector(-2._f, -2._f, -2._f), Vector(1._f, 1._f, 1._f));
    REQUIRE(intersectBox(box, ray3, t_min, t_max));
    REQUIRE(t_min == 2._f);
    REQUIRE(t_max == 3._f);

    Ray ray4(Vector(0._f, 2._f, 0._f), Vector(-0.2_f, 0.2_f, 1._f));
    REQUIRE_FALSE(intersectBox(box, ray4, t_min, t_max));
}

TEST_CASE("BvhBox", "[bvh]") {
    Array<BvhBox> objects;
    objects.emplaceBack(Box(Vector(0._f, 0._f, 0._f), Vector(1._f, 1._f, 1._f)));
    objects.emplaceBack(Box(Vector(0._f, 2._f, 0._f), Vector(0.5_f, 2.5_f, 0.5_f)));
    Bvh<BvhBox> bvh;
    objects[0].userData = 1;
    objects[1].userData = 2;
    bvh.build(std::move(objects));

    IntersectionInfo intersection;
    Ray ray1(Vector(2._f, 0.5_f, 0.5_f), Vector(-1._f, 0._f, 0._f));
    REQUIRE(bvh.getIntersection(ray1, intersection));
    REQUIRE(intersection.object);
    REQUIRE(intersection.object->userData == 1);
    REQUIRE(intersection.t == 1._f);

    Ray ray2(Vector(0._f, 3._f, 0._f), Vector(0.2_f, -1._f, 0.4_f));
    REQUIRE(bvh.getIntersection(ray2, intersection));
    REQUIRE(intersection.object);
    REQUIRE(intersection.object->userData == 2);
    REQUIRE(intersection.t == 0.5_f);

    Ray ray3(Vector(-1._f, 1.8_f, 0.3_f), Vector(1._f, 0._f, 0._f));
    REQUIRE_FALSE(bvh.getIntersection(ray3, intersection));
}

TEST_CASE("BvhSphere", "[bvh]") {
    BvhSphere sphere(Vector(0._f), 2._f);
    Ray ray1(Vector(0._f, -3._f, 0._f), Vector(0._f, 1._f, 0._f));
    IntersectionInfo intersection;
    REQUIRE(sphere.getIntersection(ray1, intersection));
    REQUIRE(intersection.t == 1._f);
    REQUIRE(intersection.object);
}

TEST_CASE("Bvh many boxes", "[bvh]") {
    Array<BvhBox> objects;
    VectorRng<UniformRng> rng;
    for (Size i = 0; i < 10000; ++i) {
        const Vector q = 10._f * rng();
        objects.emplaceBack(Box(q, q + rng() * 1._f));
    }
    Bvh<BvhBox> bvh;
    bvh.build(std::move(objects));

    IntersectionInfo intersection;
    Ray ray(Vector(-1._f, 5._f, 5._f), Vector(1._f, 0._f, 0.1_f));
    // just test that we hit something
    REQUIRE(bvh.getIntersection(ray, intersection));
    REQUIRE(intersection.t > 1._f);
    REQUIRE(intersection.t < 5._f);
    REQUIRE(intersection.object != nullptr);
}

TEST_CASE("Bvh many spheres", "[bvh]") {
    Array<BvhSphere> objects;
    VectorRng<UniformRng> rng;
    for (Size i = 0; i < 10000; ++i) {
        objects.emplaceBack(10._f * rng(), 0.25_f * rng.getAdditional(3));
    }
    Bvh<BvhSphere> bvh;
    bvh.build(std::move(objects));

    IntersectionInfo intersection;
    Ray ray(Vector(-1._f, 5._f, 5._f), Vector(1._f, 0._f, 0.1_f));
    REQUIRE(bvh.getIntersection(ray, intersection));
    REQUIRE(intersection.t > 1._f);
    REQUIRE(intersection.t < 5._f);
    REQUIRE(intersection.object != nullptr);
}
