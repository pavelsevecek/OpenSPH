#pragma once

/// \file MathUtils.h
/// \brief Additional math routines (with more includes).
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Assert.h"
#include "math/MathBasic.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

NAMESPACE_SPH_BEGIN

/// Small value (compared with 1) for given precision.
template <typename T>
struct Eps;

template <>
struct Eps<float> {
    constexpr static float value = 1.e-6f;
};
template <>
struct Eps<double> {
    constexpr static double value = 1.e-12f;
};

constexpr Float EPS = Eps<Float>::value;

/// Large value (compared with 1). It's safe to do basic arithmetic operations (multiply by 2, for example)
/// without worying about float overflow.
constexpr Float LARGE = 1.e20f;

/// Largest value representable by float/double. Any increase (multiplication by 2, for example) will cause
/// float overflow! Use carefully, maily for comparisons.
constexpr Float INFTY = std::numeric_limits<Float>::max();


/// Returns an approximate value of inverse square root.
/// \tparam Iters Number of iterations of the algorithm. Higher value = more accurate result.
template <typename T>
INLINE T sqrtInv(const T& f) {
    int i;
    // cast to float
    float x2 = float(f) * 0.5f;
    float y = float(f);
    memcpy(&i, &y, sizeof(float));
    i = 0x5f3759df - (i >> 1);
    memcpy(&y, &i, sizeof(float));
    return y * (1.5f - (x2 * y * y));
}

/// Returns an approximative value of square root.
template <typename T>
INLINE T sqrtApprox(const T f) {
    if (f == T(0)) {
        return T(0);
    }
    return T(1.f) / sqrtInv(f);
}


/// Return a squared value.
template <typename T>
INLINE constexpr T sqr(const T& f) noexcept {
    return f * f;
}

/// Returns true if given n is a power of 2. N must at least 1.
INLINE constexpr bool isPower2(const Size n) noexcept {
    return n >= 1 && (n & (n - 1)) == 0;
}

/// Return a squared root of a value.
template <typename T>
INLINE T sqrt(const T f) {
    ASSERT(f >= T(0), f);
    return std::sqrt(f);
}

/// Returns a cubed root of a value.
INLINE Float cbrt(const Float f) {
    return std::cbrt(f);
}

/// Returns a positive modulo value
INLINE int positiveMod(const int i, const int n) {
    return (i % n + n) % n;
}


template <int N>
INLINE Float root(const Float f);
template <>
INLINE Float root<1>(const Float f) {
    return f;
}
template <>
INLINE Float root<2>(const Float f) {
    return sqrt(f);
}
template <>
INLINE Float root<3>(const Float f) {
    return cbrt(f);
}
template <>
INLINE Float root<4>(const Float f) {
    /// \todo is this faster than pow(f, 0.25)?
    return sqrt(sqrt(f));
}


/// Power for floats
template <int N>
INLINE constexpr Float pow(const Float v);

template <>
INLINE constexpr Float pow<0>(const Float) {
    return 1._f;
}
template <>
INLINE constexpr Float pow<1>(const Float v) {
    return v;
}
template <>
INLINE constexpr Float pow<2>(const Float v) {
    return v * v;
}
template <>
INLINE constexpr Float pow<3>(const Float v) {
    return v * v * v;
}
template <>
INLINE constexpr Float pow<4>(const Float v) {
    return pow<2>(pow<2>(v));
}
template <>
INLINE constexpr Float pow<5>(const Float v) {
    return pow<2>(pow<2>(v)) * v;
}
template <>
INLINE constexpr Float pow<6>(const Float v) {
    return pow<3>(pow<2>(v));
}
template <>
INLINE constexpr Float pow<7>(const Float v) {
    return pow<6>(v) * v;
}
template <>
INLINE constexpr Float pow<8>(const Float v) {
    return pow<4>(pow<2>(v));
}
template <>
INLINE constexpr Float pow<-1>(const Float v) {
    return 1._f / v;
}
template <>
INLINE constexpr Float pow<-2>(const Float v) {
    return 1._f / (v * v);
}
template <>
INLINE constexpr Float pow<-3>(const Float v) {
    return 1._f / (v * v * v);
}
template <>
INLINE constexpr Float pow<-4>(const Float v) {
    return 1._f / (pow<2>(pow<2>(v)));
}
template <>
INLINE constexpr Float pow<-5>(const Float v) {
    return 1._f / (v * pow<2>(pow<2>(v)));
}
template <>
INLINE constexpr Float pow<-8>(const Float v) {
    return 1._f / (pow<2>(pow<4>(v)));
}
template <>
INLINE constexpr Float pow<-16>(const Float v) {
    return 1._f / (pow<2>(pow<8>(v)));
}


/// Power for ints
template <int N>
INLINE constexpr Size pow(const Size v);

template <>
INLINE constexpr Size pow<0>(const Size) {
    return 1;
}
template <>
INLINE constexpr Size pow<1>(const Size v) {
    return v;
}
template <>
INLINE constexpr Size pow<2>(const Size v) {
    return v * v;
}
template <>
INLINE constexpr Size pow<3>(const Size v) {
    return v * v * v;
}
template <>
INLINE constexpr Size pow<4>(const Size v) {
    return pow<2>(pow<2>(v));
}
template <>
INLINE constexpr Size pow<5>(const Size v) {
    return pow<2>(pow<2>(v)) * v;
}
template <>
INLINE constexpr Size pow<6>(const Size v) {
    return pow<3>(pow<2>(v));
}


/// Mathematical functions

template <typename T>
INLINE T pow(const T value, const T power) {
    return std::pow(value, power);
}

/// Approximative version of pow function.
///
/// Expected error is about 5 %. Works only for positive values and positive powers.
INLINE Float powFastest(const Float value, const Float power) {
    ASSERT(value > 0._f && power > 0._f, value, power);
    // https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
    // this type-punning through union is undefined behavior according to C++ standard, but most compilers
    // behave expectably ...
    union {
        double d;
        int x[2];
    } u = { value };
    u.x[1] = (int)(power * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;
    return u.d;
}

/// Approximative version of pow function, slightly more precise than powFastest.
///
/// Expected error is about 2 %. Works only for positive values and positive powers.
INLINE Float powFast(Float value, const Float power) {
    ASSERT(value > 0._f && power > 0._f, value, power);
    // https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
    int e = (int)power;
    union {
        double d;
        int x[2];
    } u = { value };
    u.x[1] = (int)((power - e) * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;

    double r = 1.0;
    while (e) {
        if (e & 1) {
            r *= value;
        }
        value *= value;
        e >>= 1;
    }
    return r * u.d;
}

template <typename T>
INLINE T exp(const T f) {
    return ::exp(f);
}

/// Computes absolute value.
/// \note Return type must be auto as abs(TracelessTensor) != TracelessTensor
template <typename T>
INLINE auto abs(const T& f) {
    return ::fabs(f);
}

template <>
INLINE auto abs(const int& f) {
    return int(::abs(f));
}

template <>
INLINE auto abs(const Size& f) {
    return Size(f);
}

template <typename T>
INLINE T cos(const T f) {
    return std::cos(f);
}

template <typename T>
INLINE T sin(const T f) {
    return std::sin(f);
}

template <typename T>
INLINE T acos(const T f) {
    return std::acos(f);
}

template <typename T>
INLINE T asin(const T f) {
    return std::asin(f);
}

template <typename T>
INLINE int sgn(const T val) {
    return (T(0) < val) - (val < T(0));
}

template <typename T, typename TAmount>
INLINE T lerp(const T v1, const T v2, const TAmount amount) {
    return v1 * (TAmount(1) - amount) + v2 * amount;
}


/// Mathematical constants
constexpr Float PI = 3.14159265358979323846264338327950288419716939937510582097_f;

constexpr Float PI_INV = 1._f / PI;

constexpr Float E = 2.718281828459045235360287471352662497757247093699959574967_f;

constexpr Float SQRT_3 = 1.732050807568877293527446341505872366942805253810380628055_f;

constexpr Float DEG_TO_RAD = PI / 180._f;

constexpr Float RAD_TO_DEG = 180._f / PI;


/// Computes a volume of a sphere given its radius.
INLINE constexpr Float sphereVolume(const Float radius) {
    return 1.3333333333333333333333_f * PI * pow<3>(radius);
}

/// Checks if two values are equal to some given accuracy.
/// \note We use <= rather than < on purpose as EPS for integral types is zero.
INLINE auto almostEqual(const Float& f1, const Float& f2, const Float& eps = EPS) {
    return abs(f1 - f2) <= eps * (1._f + max(abs(f1), abs(f2)));
}

NAMESPACE_SPH_END
