#include "sph/kernel/GravityKernel.h"
#include "catch.hpp"
#include "math/Integrator.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("Default kernel", "[kernel]") {
    GravityLutKernel kernel;
    REQUIRE(kernel.closeRadius() == 0._f);
    REQUIRE(kernel.value(Vector(2._f, 0._f, 0._f), 1._f) == -0.5_f);
    REQUIRE(kernel.grad(Vector(2._f, 0._f, 0._f), 1._f) == Vector(0.25_f, 0._f, 0._f));
}

TEST_CASE("M4 gravity kernel", "[kernel]") {
    GravityLutKernel kernel = GravityKernel<CubicSpline<3>>();
    REQUIRE(kernel.value(Vector(1._f, 0._f, 0._f), 0.1_f) < 0._f);
    REQUIRE(kernel.value(Vector(3._f, 0._f, 0._f), 5._f) < 0._f);
    REQUIRE(kernel.value(Vector(0._f, 5._f, 0._f), 1._f) == -0.2_f);
    REQUIRE(kernel.value(Vector(0._f, 5._f, 0._f), EPS) == -0.2_f);

    REQUIRE_NOTHROW(kernel.grad(Vector(1._f, 0._f, 0._f), 1._f));
    REQUIRE(kernel.grad(Vector(0._f), 1._f) == Vector(0._f));
    REQUIRE(kernel.grad(Vector(0._f, 0._f, 5._f), 1._f) == approx(Vector(0._f, 0._f, 0.04_f)));
    REQUIRE(kernel.grad(Vector(0._f, 0._f, 5._f), EPS) == approx(Vector(0._f, 0._f, 0.04_f)));

    // check that value = int grad dx

    auto test = [&](const Float x1, const Float x2, const Float h) {
        const Float lhs = integrate(Range(x1, x2), [&](const Float r) { //
            return kernel.grad(Vector(0._f, r, 0._f), h)[Y];
        });
        const Float rhs = kernel.value(Vector(0._f, x2, 0._f), h) - kernel.value(Vector(0._f, x1, 0._f), h);
        REQUIRE(lhs == approx(rhs, 1.e-6_f));
    };
    test(0._f, 3._f, 1._f);
    test(0.2_f, 0.25_f, 0.1_f);
    test(0.2_f, 5._f, 0.5_f);
    test(1._f, 6._f, 2._f);
}

TEST_CASE("M4 gravity kernel consistency", "[kernel]") {
    GravityLutKernel kernel = GravityKernel<CubicSpline<3>>();
    CubicSpline<3> m4;
    // check consistency of kernels; gravity kernel g must satisfy:
    // W = 1/(4 pi r^2)*d/dr(r^2 dg/dr)
    const Float x1 = 0.3_f;
    const Float x2 = 2.5_f;
    const Float lhs = integrate(Range(x1, x2), [&](const Float r) { //
        return 4._f * PI * sqr(r) * m4.value(Vector(r, 0._f, 0._f), 1._f);
    });
    const Float rhs = sqr(x2) * kernel.grad(Vector(x2, 0._f, 0._f), 1._f)[X] -
                      sqr(x1) * kernel.grad(Vector(x1, 0._f, 0._f), 1._f)[X];
    REQUIRE(lhs == approx(rhs, 1.e-6_f));
}
