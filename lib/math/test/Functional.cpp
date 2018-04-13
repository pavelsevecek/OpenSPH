#include "math/Functional.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Simpson's rule", "[functional]") {
    const Float two = integrate(Interval(0._f, PI), [](const Float x) { return Sph::sin(x); });
    REQUIRE(two == approx(2._f, 1.e-6_f));

    const Float log11 = integrate(Interval(0._f, 10._f), [](const Float x) { return 1._f / (1._f + x); });
    REQUIRE(log11 == approx(log(11._f), 1.e-6_f));
}

TEST_CASE("MC Integrator", "[functional]") {
    const Float targetError = 1.e-3_f;

    // Area of circle / volume of sphere
    AutoPtr<SphericalDomain> sphere = makeAuto<SphericalDomain>(Vector(0._f), 1._f);
    Integrator<> int1(std::move(sphere));
    Float result = int1.integrate([](const Vector& UNUSED(v)) -> Float { return 1._f; }, targetError);
    REQUIRE(result == approx(sphereVolume(1._f), targetError));

    // Block [0,1]^d, linear function in each component
    AutoPtr<BlockDomain> block = makeAuto<BlockDomain>(Vector(0.5_f), Vector(1._f));
    Integrator<> int2(std::move(block));
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

TEST_CASE("GetRoots", "[functional]") {
    Optional<Float> root = getRoot(Interval(0._f, PI), EPS, [](const Float x) { return cos(x); });
    REQUIRE(root);
    REQUIRE(root.value() == approx(0.5_f * PI));

    REQUIRE_FALSE(getRoot(Interval(0._f, 1._f), EPS, [](const Float) { return 1._f; }));
}
