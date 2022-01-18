#include "objects/finders/Bvh.h"
#include "catch.hpp"
#include "math/rng/VectorRng.h"

using namespace Sph;

TEST_CASE("Bvh box intersect", "[bvh]") {
    Box box(Vector(0._f), Vector(1._f));

    Ray ray1(Vector(2._f, 0.5_f, 0.5_f), Vector(-1._f, 0._f, 0._f));
    Interval segment;
    REQUIRE(intersectBox(box, ray1, segment));
    REQUIRE(segment.lower() == 1._f);
    REQUIRE(segment.upper() == 2._f);

    // same ray, different parametrization
    Ray ray2(Vector(2._f, 0.5_f, 0.5_f), Vector(-0.5_f, 0._f, 0._f));
    REQUIRE(intersectBox(box, ray2, segment));
    REQUIRE(segment.lower() == 2._f);
    REQUIRE(segment.upper() == 4._f);

    Ray ray3(Vector(-2._f, -2._f, -2._f), Vector(1._f, 1._f, 1._f));
    REQUIRE(intersectBox(box, ray3, segment));
    REQUIRE(segment.lower() == 2._f);
    REQUIRE(segment.upper() == 3._f);

    Ray ray4(Vector(0._f, 2._f, 0._f), Vector(-0.2_f, 0.2_f, 1._f));
    REQUIRE_FALSE(intersectBox(box, ray4, segment));
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
    REQUIRE(bvh.getFirstIntersection(ray1, intersection));
    REQUIRE(intersection.object);
    REQUIRE(intersection.object->userData == 1);
    REQUIRE(intersection.t == 1._f);

    Ray ray2(Vector(0._f, 3._f, 0._f), Vector(0.2_f, -1._f, 0.4_f));
    REQUIRE(bvh.getFirstIntersection(ray2, intersection));
    REQUIRE(intersection.object);
    REQUIRE(intersection.object->userData == 2);
    REQUIRE(intersection.t == 0.5_f);

    Ray ray3(Vector(-1._f, 1.8_f, 0.3_f), Vector(1._f, 0._f, 0._f));
    REQUIRE_FALSE(bvh.getFirstIntersection(ray3, intersection));

    REQUIRE(bvh.getBoundingBox() == Box(Vector(0._f, 0._f, 0._f), Vector(1._f, 2.5_f, 1._f)));
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
    Box bbox;
    for (Size i = 0; i < 10000; ++i) {
        const Vector q = 10._f * rng();
        Box box(q, q + rng() * 1._f);
        objects.emplaceBack(box);
        bbox.extend(box);
    }
    Bvh<BvhBox> bvh;
    bvh.build(std::move(objects));

    IntersectionInfo intersection;
    Ray ray(Vector(-1._f, 5._f, 5._f), Vector(1._f, 0._f, 0.1_f));
    // just test that we hit something
    REQUIRE(bvh.getFirstIntersection(ray, intersection));
    REQUIRE(intersection.t > 1._f);
    REQUIRE(intersection.t < 5._f);
    REQUIRE(intersection.object != nullptr);

    REQUIRE(bvh.getBoundingBox() == bbox);
}

TEST_CASE("Bvh many spheres", "[bvh]") {
    Array<BvhSphere> objects;
    // using BenzAsphaugRng to avoid dependency on the current compiler version
    VectorRng<BenzAsphaugRng> rng(1234);
    for (Size i = 0; i < 10000; ++i) {
        // spheres with radius up to 0.25, randomly distributed in box [0, 10]
        objects.emplaceBack(10._f * rng(), 0.25_f * rng.getAdditional(3));
    }
    Bvh<BvhSphere> bvh;
    bvh.build(std::move(objects));

    IntersectionInfo intersection;
    Ray ray(Vector(-1._f, 5._f, 5._f), Vector(1._f, 0._f, 0.1_f));
    REQUIRE(bvh.getFirstIntersection(ray, intersection));
    REQUIRE(intersection.t > 1._f);
    REQUIRE(intersection.t < 5._f);
    REQUIRE(intersection.object != nullptr);
}
