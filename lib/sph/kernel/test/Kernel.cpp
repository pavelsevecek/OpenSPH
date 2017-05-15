#include "sph/kernel/Kernel.h"
#include "catch.hpp"
#include "math/Integrator.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

/// Test given kernel and its approximation given by LUT
template <int d, typename TKernel, typename TTest>
void testKernel(const TKernel& kernel, TTest&& test) {
    // compact support
    Float radiusSqr = sqr(kernel.radius());

    REQUIRE(kernel.valueImpl(radiusSqr) == 0._f);
    REQUIRE(kernel.valueImpl(radiusSqr * 1.1_f) == 0._f);
    REQUIRE(kernel.valueImpl(radiusSqr * 0.9_f) > 0._f);

    // normalization
    const Float targetError = 1.e-3_f;
    SphericalDomain domain(Vector(0._f), kernel.radius());
    Integrator<> in(domain);
    Float norm = in.integrate([&](const Vector& v) { return kernel.value(v, 1._f); }, targetError);
    REQUIRE(norm == approx(1._f, 5._f * kernel.radius() * targetError));

    // check that kernel gradients match (approximately) finite differences of values
    // fine-tuned for floats, maximum accuracy (lower - round-off errors, higher - imprecise derivative)
    Float eps = 0.0003_f;
    for (Float x = eps; x < kernel.radius(); x += 0.2_f) {
        Float xSqr = x * x;
        Float diff = (kernel.valueImpl(sqr(x + eps)) - kernel.valueImpl(sqr(x - eps))) / (2 * eps);
        REQUIRE(kernel.gradImpl(xSqr) * x == approx(diff, 2._f * eps));
    }

    // check that kernel and LUT give the same values and gradients
    LutKernel<d> lut(kernel);
    // check that its values match the precise kernels
    const Size testCnt = Size(kernel.radius() / 0.001_f);
    auto test1 = [&](const Size i) -> Outcome {
        Float x = i * 0.5001_f;
        // clang-format off
        if (lut.valueImpl(sqr(x)) != approx(kernel.valueImpl(sqr(x)), 1.e-6_f)) {
            return makeFailed("LUT not matching kernel at q = ", x,
                              "\n ", lut.valueImpl(sqr(x)), " == ", kernel.valueImpl(sqr(x)));
        }
        if (lut.gradImpl(sqr(x)) != approx(kernel.gradImpl(sqr(x)), 1.e-6_f)) {
            return makeFailed("LUT gradient not matching kernel gradient at q = ", x,
                              "\n ", lut.gradImpl(sqr(x)), " == ", kernel.gradImpl(sqr(x)));
        }
        // clang-format on
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, testCnt);

    // run given tests for both the kernel and LUT
    test(kernel);
    test(lut);
}

TEST_CASE("M4 kernel", "[kernel]") {
    CubicSpline<3> m4;

    testKernel<3>(m4, [](auto&& kernel) {
        REQUIRE(kernel.radius() == 2._f);
        Float norm = 1. / PI;

        // specific points from kernel
        REQUIRE(kernel.valueImpl(0._f) == norm);
        REQUIRE(kernel.valueImpl(1._f) == approx(0.25_f * norm));
        REQUIRE(kernel.gradImpl(1._f) == approx(-0.75_f * norm));
    });

    CubicSpline<1> m4_1d;
    // 1D norm
    const Float norm1 = integrate(Range(0._f, 2._f), [&](const Float x) { //
        return m4_1d.valueImpl(sqr(x));
    });
    LutKernel<1> lut(m4_1d);
    const Float norm2 = integrate(Range(0._f, 2._f), [&](const Float x) { //
        return lut.valueImpl(sqr(x));
    });
    // we only integrate 1/2 of the 1D kernel (support is [-2, 2])
    REQUIRE(norm1 == approx(0.5_f, 1.e-6_f));
    REQUIRE(norm2 == approx(0.5_f, 1.e-6_f));

    const Float grad1 = integrate(Range(0._f, 2._f), [&](const Float x) { //
        return x * m4_1d.gradImpl(sqr(x));
    });
    const Float grad2 = integrate(Range(0._f, 2._f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    const Float grad11 = integrate(Range(0._f, 1._f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    const Float grad12 = integrate(Range(1._f, 2._f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    REQUIRE(grad1 == approx(-2._f / 3._f, 1.e-6_f));
    REQUIRE(grad2 == approx(-2._f / 3._f, 1.e-6_f));
    REQUIRE(grad11 == approx(-0.5_f, 1.e-6_f));
    REQUIRE(grad12 == approx(-1._f / 6._f, 1.e-6_f));
}


TEST_CASE("M5 kernel", "[kernel]") {
    FourthOrderSpline<3> m5;


    testKernel<3>(m5, [](auto&& kernel) { REQUIRE(kernel.radius() == 2.5_f); });

    FourthOrderSpline<1> m5_1d;
    // 1D norm
    const Float norm1 = integrate(Range(0._f, 2.5_f), [&](const Float x) { return m5_1d.valueImpl(sqr(x)); });
    LutKernel<1> lut(m5_1d);
    const Float norm2 = integrate(Range(0._f, 2.5_f), [&](const Float x) { return lut.valueImpl(sqr(x)); });
    // we only integrate 1/2 of the 1D kernel (support is [-2.5, 2.5])
    REQUIRE(almostEqual(norm1, 0.5_f, 1.e-6_f));
    REQUIRE(almostEqual(norm2, 0.5_f, 1.e-6_f));

    const Float grad1 = integrate(Range(0._f, 2.5_f), [&](const Float x) { //
        return x * m5_1d.gradImpl(sqr(x));
    });
    const Float grad2 = integrate(Range(0._f, 2.5_f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    REQUIRE(almostEqual(grad1, -115._f / 192._f, 1.e-6_f));
    REQUIRE(almostEqual(grad2, -115._f / 192._f, 1.e-6_f));
}


TEST_CASE("Gaussian kernel", "[kernel]") {
    Gaussian<3> gaussian;
    testKernel<3>(gaussian, [](auto&& kernel) { REQUIRE(kernel.radius() == 5._f); });
}

TEST_CASE("Lut kernel", "[kernel]") {
    // test that LUT is moveable
    LutKernel<3> lut(CubicSpline<3>{});
    const Float value = lut.value(Vector(1.2_f, 0._f, 0._f), 0.9_f);
    const Vector grad = lut.grad(Vector(0.8_f, 0._f, 0._f), 1.5_f);

    LutKernel<3> lut2 = std::move(lut);
    REQUIRE(lut2.value(Vector(1.2_f, 0._f, 0._f), 0.9_f) == value);
    REQUIRE(lut2.grad(Vector(0.8_f, 0._f, 0._f), 1.5_f) == grad);
    REQUIRE(lut2.radius() == 2._f);

    LutKernel<3> lut3;
    lut3 = std::move(lut2);
    REQUIRE(lut3.value(Vector(1.2_f, 0._f, 0._f), 0.9_f) == value);
    REQUIRE(lut3.grad(Vector(0.8_f, 0._f, 0._f), 1.5_f) == grad);
    REQUIRE(lut3.radius() == 2._f);
}
