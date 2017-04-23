#pragma once

/// Basic math routines used within the code. Use "" functions instead of std:: ones, for consistency.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "common/Assert.h"
#include "common/Globals.h"
#include "common/Traits.h"
#include <algorithm>
#include <cstring>
#include <math.h>
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
template <>
struct Eps<__float128> {
    constexpr static __float128 value = 1.e-18f;
};

constexpr Float EPS = Eps<Float>::value;

/// Large value (compared with 1). It's safe to do basic arithmetic operations (multiply by 2, for example)
/// without worying about float overflow.
constexpr Float LARGE = 1.e20f;

/// Largest value representable by float/double. Any increase (multiplication by 2, for example) will cause
/// float overflow! Use carefully, maily for comparisons.
constexpr Float INFTY = std::numeric_limits<Float>::max();


/// Minimum & Maximum value
template <typename T>
INLINE constexpr T max(const T& f1, const T& f2) {
    return (f1 > f2) ? f1 : f2;
}

template <typename T>
INLINE constexpr T min(const T& f1, const T& f2) {
    return (f1 < f2) ? f1 : f2;
}

template <typename T>
INLINE constexpr T min(const T& f1, const T& f2, const T& f3) {
    return min(min(f1, f2), f3);
}

template <typename T>
INLINE constexpr T max(const T& f1, const T& f2, const T& f3) {
    return max(max(f1, f2), f3);
}

template <typename T>
INLINE constexpr T clamp(const T& f, const T& f1, const T& f2) {
    return max(f1, min(f, f2));
}


/// Returns an approximate value of inverse square root.
/// \tparam Iters Number of iterations of the algorithm. Higher value = more accurate result.
template <typename T>
INLINE T sqrtInv(const T& f) {
    int i;
    // cast to float
    float x2 = float(f) * 0.5f;
    float y = f;
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
INLINE constexpr T sqr(const T& f) {
    return f * f;
}

/// Returns true if given n is a power of 2. N must at least 1, function does not check the input.
INLINE constexpr bool isPower2(const Size n) {
    return (n & (n - 1)) == 0;
}


/*template <typename T>
INLINE T pow(const T f, const T e) {
    return ::pow(f, e);
}*/


/*namespace Detail {
    // default implementation using (slow) pow function.
    template <int n>
    struct GetRoot {
        // forcing type deduction
        template <typename T>
        INLINE static auto value(T&& v) {
            return pow(v, 1.f / n);
        }
    };

    // specialization for square root
    template <>
    struct GetRoot<2> {
        template <typename T>
        INLINE static auto value(T&& v) {
            return std::sqrt(v);
        }
    };
    // specialization for cube root
    template <>
    struct GetRoot<3> {
        template <typename T>
        INLINE static auto value(T&& v) {
            return std::cbrt(v);
        }
    };
}*/


/// Return a squared root of a value.
template <typename T>
INLINE T sqrt(const T f) {
    ASSERT(f >= 0._f);
    return std::sqrt(f);
}

/// Returns a cubed root of a value.
INLINE Float cbrt(const Float f) {
    return std::cbrt(f);
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
    return Size(::abs(f));
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


/// Mathematical constants
constexpr Float PI = 3.14159265358979323846264338327950288419716939937510582097_f;

constexpr Float PI_INV = 1._f / PI;

constexpr Float E = 2.718281828459045235360287471352662497757247093699959574967_f;

constexpr Float SQRT_3 = 1.732050807568877293527446341505872366942805253810380628055_f;


/// Computes a volume of a sphere given its radius.
INLINE Float sphereVolume(const Float radius) {
    return 1.3333333333333333333333_f * PI * pow<3>(radius);
}

/// Checks if two values are equal to some given accuracy.
/// \note We use <= rather than < on purpose as EPS for integral types is zero.
INLINE auto almostEqual(const Float& f1, const Float& f2, const Float& eps = EPS) {
    return abs(f1 - f2) <= eps * (1._f + max(abs(f1), abs(f2)));
}


/// Function needed for generic operation with types in quantities. Must be specialized by every object stored
/// in quantity.

/// Returns a norm, absolute value by default.
template <typename T>
INLINE Float norm(const T& value) {
    return abs(value);
}

/// Squared value of the norm.
template <typename T>
INLINE Float normSqr(const T& value) {
    return sqr(value);
}

/// Returns minimum element, simply the value iself by default. This function is intended for vectors and
/// tensors, function for float is only for writing generic code.
template <typename T>
INLINE Float minElement(const T& value) {
    return value;
}

/// Checks for nans and infs.
template <typename T>
INLINE bool isReal(const T& value) {
    return !std::isnan(value) && !std::isinf(value);
}

/// Compares two objects of the same time component-wise. Returns object containing components 0 or 1,
/// depending whether components of the first objects are smaller than components of the second object.
/// \note The return type can be generally different if the mask cannot be represented using type T.
template <typename T>
INLINE constexpr auto less(const T& v1, const T& v2) {
    return v1 < v2 ? T(1) : T(0);
}


NAMESPACE_SPH_END
