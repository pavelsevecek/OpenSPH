#include "sph/kernel/Kernel.h"
#include "catch.hpp"
#include "math/Functional.h"
#include "objects/wrappers/Flags.h"
#include "tests/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

enum class KernelTestFlag {
    /// Tests that the integral of gradient is 1
    NORMALIZATION = 1 << 0,

    /// Tests that the derivative computed by finite differences approximately matches the gradient
    VALUE_GRADIENT_CONSISTENCY = 1 << 1,

    /// Checks that kernel values are continuous for q>0
    VALUES_CONTINUOUS = 1 << 2,

    /// Checks that kernel gradient is continuous for q>0
    GRADIENT_CONTINUOUS = 1 << 3,

    /// Checks that the gradient is continuous at 0
    GRADIENT_CONTINUOUS_AT_0 = 1 << 4,

    /// Checks that the exact value approximately matches value from LUT
    EQUALS_LUT = 1 << 5,
};

const auto ALL_TEST_FLAGS = KernelTestFlag::NORMALIZATION | KernelTestFlag::VALUE_GRADIENT_CONSISTENCY |
                            KernelTestFlag::VALUES_CONTINUOUS | KernelTestFlag::GRADIENT_CONTINUOUS |
                            KernelTestFlag::GRADIENT_CONTINUOUS_AT_0 | KernelTestFlag::EQUALS_LUT;

/// Test given kernel and its approximation given by LUT
template <int d, typename TKernel, typename TTest>
void testKernel(const TKernel& kernel,
    TTest&& test,
    Flags<KernelTestFlag> flags = ALL_TEST_FLAGS,
    const Float continuousEps = 0.015_f) {
    // compact support
    Float radiusSqr = sqr(kernel.radius());

    REQUIRE(kernel.valueImpl(radiusSqr) == 0._f);
    REQUIRE(kernel.valueImpl(radiusSqr * 1.1_f) == 0._f);
    REQUIRE(kernel.valueImpl(radiusSqr * 0.9_f) > 0._f);

    // normalization
    if (flags.has(KernelTestFlag::NORMALIZATION)) {
        const Float targetError = 1.e-3_f;
        Integrator<> in(makeAuto<SphericalDomain>(Vector(0._f), kernel.radius()));
        Float norm = in.integrate([&](const Vector& v) { return kernel.value(v, 1._f); }, targetError);
        REQUIRE(norm == approx(1._f, 5._f * kernel.radius() * targetError));
    }

    // check that kernel gradients match (approximately) finite differences of values
    // fine-tuned for floats, maximum accuracy (lower - round-off errors, higher - imprecise derivative)
    if (flags.has(KernelTestFlag::VALUE_GRADIENT_CONSISTENCY)) {
        Float eps = 0.0003_f;
        for (Float x = eps; x < kernel.radius(); x += 0.2_f) {
            Float xSqr = x * x;
            Float diff = (kernel.valueImpl(sqr(x + eps)) - kernel.valueImpl(sqr(x - eps))) / (2 * eps);
            REQUIRE(kernel.gradImpl(xSqr) * x == approx(diff, 2._f * eps));
        }
    }

    // check that kernel and LUT give the same values and gradients
    LutKernel<d> lut(kernel);

    // check that kernel gradient is continuous at q->0
    if (flags.has(KernelTestFlag::GRADIENT_CONTINUOUS_AT_0)) {
        REQUIRE(kernel.gradImpl(0._f) == approx(kernel.gradImpl(1.e-8_f), 1.e-3_f));
        REQUIRE(lut.gradImpl(0._f) == approx(lut.gradImpl(1.e-8_f), 1.e-3_f));
    }

    if (flags.has(KernelTestFlag::VALUES_CONTINUOUS)) {
        // values have to be always continuous in the whole interval
        REQUIRE(
            isContinuous(Interval(0._f, kernel.radius() + 0.1_f), 0.01_f, continuousEps, [&kernel](Float q) {
                return kernel.valueImpl(sqr(q));
            }));
        REQUIRE(isContinuous(Interval(0._f, lut.radius() + 0.1_f), 0.01_f, continuousEps, [&lut](Float q) {
            return lut.valueImpl(sqr(q));
        }));
    }

    if (flags.has(KernelTestFlag::GRADIENT_CONTINUOUS)) {
        // gradient does not have to be continuous close to 0, hence the other test
        REQUIRE(
            isContinuous(Interval(0.1_f, kernel.radius() + 0.1_f), 0.01_f, continuousEps, [&kernel](Float q) {
                return q * kernel.gradImpl(sqr(q));
            }));
        REQUIRE(isContinuous(Interval(0.1_f, lut.radius() + 0.1_f), 0.01_f, continuousEps, [&lut](Float q) {
            return q * lut.gradImpl(sqr(q));
        }));
    }

    if (flags.has(KernelTestFlag::EQUALS_LUT)) {
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
    }

    // run given tests for both the kernel and LUT
    test(kernel);
    test(lut);
}

TEST_CASE("M4 kernel", "[kernel]") {
    CubicSpline<3> m4;

    testKernel<3>(m4, [](const auto& kernel) {
        REQUIRE(kernel.radius() == 2._f);
        Float norm = 1. / PI;

        // specific points from kernel
        REQUIRE(kernel.valueImpl(0._f) == norm);
        REQUIRE(kernel.valueImpl(1._f) == approx(0.25_f * norm));
        REQUIRE(kernel.gradImpl(1._f) == approx(-0.75_f * norm));
    });

    CubicSpline<1> m4_1d;
    // 1D norm
    const Float norm1 = integrate(Interval(0._f, 2._f), [&](const Float x) { //
        return m4_1d.valueImpl(sqr(x));
    });
    LutKernel<1> lut(m4_1d);
    const Float norm2 = integrate(Interval(0._f, 2._f), [&](const Float x) { //
        return lut.valueImpl(sqr(x));
    });
    // we only integrate 1/2 of the 1D kernel (support is [-2, 2])
    REQUIRE(norm1 == approx(0.5_f, 1.e-6_f));
    REQUIRE(norm2 == approx(0.5_f, 1.e-6_f));

    const Float grad1 = integrate(Interval(0._f, 2._f), [&](const Float x) { //
        return x * m4_1d.gradImpl(sqr(x));
    });
    const Float grad2 = integrate(Interval(0._f, 2._f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    const Float grad11 = integrate(Interval(0._f, 1._f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    const Float grad12 = integrate(Interval(1._f, 2._f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    REQUIRE(grad1 == approx(-2._f / 3._f, 1.e-6_f));
    REQUIRE(grad2 == approx(-2._f / 3._f, 1.e-6_f));
    REQUIRE(grad11 == approx(-0.5_f, 1.e-6_f));
    REQUIRE(grad12 == approx(-1._f / 6._f, 1.e-6_f));
}


TEST_CASE("M5 kernel", "[kernel]") {
    FourthOrderSpline<3> m5;

    testKernel<3>(m5, [](const auto& kernel) { REQUIRE(kernel.radius() == 2.5_f); });

    FourthOrderSpline<1> m5_1d;
    // 1D norm
    const Float norm1 = integrate(Interval(0._f, 2.5_f), [&](const Float x) { //
        return m5_1d.valueImpl(sqr(x));
    });
    LutKernel<1> lut(m5_1d);
    const Float norm2 = integrate(Interval(0._f, 2.5_f), [&](const Float x) { //
        return lut.valueImpl(sqr(x));
    });
    // we only integrate 1/2 of the 1D kernel (support is [-2.5, 2.5])
    REQUIRE(almostEqual(norm1, 0.5_f, 1.e-6_f));
    REQUIRE(almostEqual(norm2, 0.5_f, 1.e-6_f));

    const Float grad1 = integrate(Interval(0._f, 2.5_f), [&](const Float x) { //
        return x * m5_1d.gradImpl(sqr(x));
    });
    const Float grad2 = integrate(Interval(0._f, 2.5_f), [&](const Float x) { //
        return x * lut.gradImpl(sqr(x));
    });
    REQUIRE(almostEqual(grad1, -115._f / 192._f, 1.e-6_f));
    REQUIRE(almostEqual(grad2, -115._f / 192._f, 1.e-6_f));
}


TEST_CASE("Gaussian kernel", "[kernel]") {
    Gaussian<3> gaussian;
    testKernel<3>(gaussian, [](const auto& kernel) { REQUIRE(kernel.radius() == 5._f); });
}

TEST_CASE("Wendland C2 kernel", "[kernel]") {
    WendlandC2 kernel;
    testKernel<3>(kernel, [](const auto& kernel) { REQUIRE(kernel.radius() == 2._f); });
}

TEST_CASE("Wendland C4 kernel", "[kernel]") {
    WendlandC4 kernel;
    testKernel<3>(
        kernel, [](const auto& kernel) { REQUIRE(kernel.radius() == 2._f); }, ALL_TEST_FLAGS, 0.03_f);
}

TEST_CASE("Wendland C6 kernel", "[kernel]") {
    WendlandC6 kernel;
    testKernel<3>(
        kernel, [](const auto& kernel) { REQUIRE(kernel.radius() == 2._f); }, ALL_TEST_FLAGS, 0.05_f);
}

TEST_CASE("Thomas-Couchman kernel", "[kernel]") {
    ThomasCouchmanKernel<3> kernel;

    // This kernel is NOT consistent on purpose, so we cannot test it. It is also discontinuous in zero (there
    // is a skip from 1 to -1).
    auto flags = KernelTestFlag::NORMALIZATION | KernelTestFlag::EQUALS_LUT |
                 KernelTestFlag::VALUES_CONTINUOUS | KernelTestFlag::GRADIENT_CONTINUOUS;

    testKernel<3>(kernel,
        [](const auto& kernel) {
            REQUIRE(kernel.radius() == 2._f);

            // check that gradient is const around 0
            REQUIRE(kernel.grad(Vector(0.1_f, 0._f, 0._f), 0.5_f) ==
                    approx(kernel.grad(Vector(0.2_f, 0._f, 0._f), 0.5_f)));
        },
        flags);
}

TEST_CASE("Triangle kernel", "[kernel]") {
    TriangleKernel<3> kernel;
    // triangle is continuous, but it has discontinuous derivatives
    auto flags = KernelTestFlag::VALUE_GRADIENT_CONSISTENCY | KernelTestFlag::NORMALIZATION |
                 KernelTestFlag::EQUALS_LUT | KernelTestFlag::VALUES_CONTINUOUS;
    testKernel<3>(kernel, [](const auto& kernel) { REQUIRE(kernel.radius() == 1._f); }, flags, 0.01_f);
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
