#include "math/Integrator.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Integrators", "[integrators]") {
    const Float targetError = 1.e-3_f;

    // Area of circle / volume of sphere
    SphericalDomain sphere(Vector(0._f), 1._f);
    Integrator<> int1(&sphere);
    Float result = int1.integrate([](const Vector& UNUSED(v)) -> Float { return 1._f; }, targetError);
    REQUIRE(almostEqual(result, sphereVolume(1._f), targetError));

    // Block [0,1]^d, linear function in each component
    BlockDomain block(Vector(0.5_f), Vector(1._f));
    Integrator<> int2(&block);
    result = int2.integrate([](const Vector& v) { return dot(v, Vector(1._f)); }, targetError);
    REQUIRE(almostEqual(result, 3 * 0.5_f, 2._f * targetError));

    // Gauss integral
    /*SphericalDomain<float, 2> circle(Vector<float, 2>(0.f), 1.f);
    Integrator<float, 2> int3(&circle);
    result =
        int3.integrate([](const Vector<float, 2>& v) { return exp(-getSqrLength(v)); }, targetError);
    REQUIRE(almostEqual(result,
                              (E<float> - 1.f) / E<float> * PI<float>,
                              2.f * targetError));*/

}
