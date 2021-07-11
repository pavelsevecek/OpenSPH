#include "sph/kernel/GravityKernel.h"
#include "catch.hpp"
#include "math/Functional.h"
#include "tests/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

static_assert(IsGravityKernel<GravityKernel<CubicSpline<3>>>::value, "GravityKernel test failed");
static_assert(IsGravityKernel<GravityKernel<ThomasCouchmanKernel<3>>>::value, "GravityKernel test failed");
static_assert(IsGravityKernel<SolidSphereKernel>::value, "GravityKernel test failed");
static_assert(!IsGravityKernel<GravityLutKernel>::value, "GravityKernel test failed");
static_assert(!IsGravityKernel<CubicSpline<3>>::value, "GravityKernel test failed");
static_assert(!IsGravityKernel<LutKernel<3>>::value, "GravityKernel test failed");

using KernelImpl = decltype(getAssociatedGravityKernel(std::declval<CubicSpline<3>>(), std::declval<Size>()));
static_assert(IsGravityKernel<KernelImpl>::value, "GravityKernel test failed");

TEST_CASE("Default kernel", "[kernel]") {
    GravityLutKernel kernel;
    REQUIRE(kernel.radius() == 0._f);
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
        const Float lhs = integrate(Interval(x1, x2), [&](const Float r) { //
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
    for (Float h : { 0.25_f, 0.5_f, 1._f, 2.3_f }) {
        const Float lhs = integrate(Interval(x1, x2), [&](const Float r) { //
            return 4._f * PI * sqr(r) * m4.value(Vector(r, 0._f, 0._f), h);
        });
        const Float rhs = sqr(x2) * kernel.grad(Vector(x2, 0._f, 0._f), h)[X] -
                          sqr(x1) * kernel.grad(Vector(x1, 0._f, 0._f), h)[X];
        REQUIRE(lhs == approx(rhs, 1.e-6_f));
    }
}

TEST_CASE("Check getAssociatedGravityKernel", "[kernel]") {
    GravityLutKernel kernel1 = GravityKernel<CubicSpline<3>>();
    GravityLutKernel kernel2 = getAssociatedGravityKernel(CubicSpline<3>());

    REQUIRE(kernel1.radius() == kernel2.radius());
    for (Float h : { 0.25_f, 0.5_f, 1._f, 2.3_f }) {
        bool valueMatches = true;
        bool gradientMatches = true;
        for (Float x = 0; x < 3._f; x += 0.01_f) {
            const Float w1 = kernel1.value(Vector(x, 0, 0), h);
            const Float w2 = kernel2.value(Vector(x, 0, 0), h);
            valueMatches &= w1 == approx(w2, 1.e-5_f);

            const Float dw1 = kernel1.grad(Vector(x, 0, 0), h)[0];
            const Float dw2 = kernel2.grad(Vector(x, 0, 0), h)[0];
            gradientMatches &= dw1 == approx(dw2, 1.e-4_f);
        }
        REQUIRE(valueMatches);
        REQUIRE(gradientMatches);
    }
}
