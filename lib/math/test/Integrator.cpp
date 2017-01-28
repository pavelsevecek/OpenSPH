#include "math/Integrator.h"
#include "catch.hpp"
#include "utils/Approx.h"

using namespace Sph;

TEST_CASE("Simpson's rule", "[integrators]") {
    const Float two = integrate(Range(0._f, PI), [](const Float x) { return Sph::sin(x); });
    REQUIRE(two == approx(2._f, 1.e-6_f));

    const Float log11 = integrate(Range(0._f, 10._f), [](const Float x) { return 1._f / (1._f + x); });
    REQUIRE(log11 == approx(log(11._f), 1.e-6_f));
}

TEST_CASE("MC Integrator", "[integrators]") {
    const Float targetError = 1.e-3_f;

    // Area of circle / volume of sphere
    SphericalDomain sphere(Vector(0._f), 1._f);
    Integrator<> int1(sphere);
    Float result = int1.integrate([](const Vector& UNUSED(v)) -> Float { return 1._f; }, targetError);
    REQUIRE(result == approx(sphereVolume(1._f), targetError));

    // Block [0,1]^d, linear function in each component
    BlockDomain block(Vector(0.5_f), Vector(1._f));
    Integrator<> int2(block);
    result = int2.integrate([](const Vector& v) { return dot(v, Vector(1._f)); }, targetError);
    REQUIRE(result == approx(3 * 0.5_f, 2._f * targetError));

    // Gauss integral
    /*SphericalDomain<float, 2> circle(Vector<float, 2>(0.f), 1.f);
    Integrator<float, 2> int3(&circle);
    result =
        int3.integrate([](const Vector<float, 2>& v) { return exp(-getSqrLength(v)); }, targetError);
    REQUIRE(almostEqual(result,
                              (E<float> - 1.f) / E<float> * PI<float>,
                              2.f * targetError));*/
}
