#include "sph/kernel/Kernel.h"
#include "catch.hpp"
#include "math/Integrator.h"
#include <iostream>

using namespace Sph;

/// Test given kernel and its approximation given by LUT
template <int d, typename TKernel, typename TTest>
void testKernel(const TKernel& kernel, TTest&& test) {
    // compact support
    Float radiusSqr = Math::sqr(kernel.radius());

    REQUIRE(kernel.valueImpl(radiusSqr) == 0._f);
    REQUIRE(kernel.valueImpl(radiusSqr * 1.1_f) == 0._f);
    REQUIRE(kernel.valueImpl(radiusSqr * 0.9_f) > 0._f);

    // normalization
    const Float targetError = 1.e-3_f;
    SphericalDomain domain(Vector(0._f), kernel.radius());
    Integrator<> in(&domain);
    Float norm = in.integrate([&](const Vector& v) { return kernel.value(v, 1._f); }, targetError);
    REQUIRE(Math::almostEqual(norm, 1._f, 3._f * targetError));

    // check that kernel gradients match (approximately) finite differences of values
    // fine-tuned for floats, maximum accuracy (lower - round-off errors, higher - imprecise derivative)
    Float eps = 0.0003_f;
    for (Float x = eps; x < kernel.radius(); x += 0.2_f) {
        Float xSqr = x * x;
        Float diff =
            (kernel.valueImpl(Math::sqr(x + eps)) - kernel.valueImpl(Math::sqr(x - eps))) / (2 * eps);
        //std::cout << kernel.gradImpl(xSqr) * x << "  |  " <<  diff << std::endl;
        REQUIRE(Math::almostEqual(kernel.gradImpl(xSqr) * x, diff, 2._f * eps));
    }

    // check that kernel and LUT give the same values and gradients
    LutKernel<d> lut(kernel);
    // check that its values match the precise M4 kernels
    for (Float x = 0._f; x < kernel.radius(); x += 0.2_f) {
        REQUIRE(Math::almostEqual(lut.valueImpl(x), kernel.valueImpl(x)));
        REQUIRE(Math::almostEqual(lut.gradImpl(x), kernel.gradImpl(x)));
    }

    // run given tests for both the kernel and LUT
    test(kernel);
    test(lut);
}

TEST_CASE("M4 kernel", "[kernel]") {
    CubicSpline<3> m4;


    testKernel<3>(m4, [](auto&& kernel) {
        REQUIRE(kernel.radius() == 2._f);
        Float norm = 1. / Math::PI;

        // specific points from kernel
        REQUIRE(kernel.valueImpl(0._f) == norm);
        std::cout << "kernel = " << kernel.valueImpl(1._f) << " / " << 0.25_f << std::endl;
        REQUIRE(Math::almostEqual(kernel.valueImpl(1._f), 0.25_f * norm));
        REQUIRE(kernel.gradImpl(0._f) == 0._f);
        REQUIRE(Math::almostEqual(kernel.gradImpl(1._f), -0.75_f * norm));
    });
}


TEST_CASE("M5 kernel", "[kernel]") {
    FourthOrderSpline<3> m5;


    testKernel<3>(m5, [](auto&& kernel) {
        REQUIRE(kernel.radius() == 2.5_f);
    });
}
