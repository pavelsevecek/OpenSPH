#pragma once

/// Basic math routines used within the code. Use "Math::" functions instead of std:: ones, for consistency.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Globals.h"
#include "core/Traits.h"
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

constexpr Float EPS = Eps<Float>::value;

/// Large value (compared with 1)
constexpr Float INFTY = 1.e20f;

namespace Math {
    template <typename T>
    INLINE constexpr auto max(const T f1, const T f2) {
        return (f1 > f2) ? f1 : f2;
    }

    template <typename T>
    INLINE constexpr auto min(const T f1, const T f2) {
        return (f1 < f2) ? f1 : f2;
    }

    template <typename T>
    INLINE constexpr auto clamp(const T f, const T f1, const T f2) {
        return max(f1, min(f, f2));
    }

    INLINE int round(const float f) { return ::round(f); }


    /// Returns an approximate value of inverse square root.
    /// \tparam Iters Number of iterations of the algorithm. Higher value = more accurate result.
    template <int iters = 1, typename T>
    INLINE T sqrtInv(const T& f) {
        int i;
        // cast to float
        float x2 = float(f) * 0.5f;
        float y  = f;
        memcpy(&i, &y, sizeof(float));
        i = 0x5f3759df - (i >> 1);
        memcpy(&y, &i, sizeof(float));
        for (int i = 0; i < iters; ++i) {
            y = y * (1.5f - (x2 * y * y));
        }
        return T(y);
    }

    template <typename T>
    INLINE T sqrtApprox(const T f) {
        return T(1.f) / sqrtInv(f);
    }

    template <typename T>
    INLINE auto sqr(T&& f) {
        return f * f;
    }
    template <typename T>
    INLINE T pow(const T f, const T e) {
        return ::pow(f, e);
    }


    namespace Detail {
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
    }

    /// Return n-th root of a value. Works with dimensional analysis, throws compile error if attempted to get
    /// a root of a unit that is not n-th power (that would result in units with rational powers, which is not
    /// supported).
    template <int n, typename T>
    INLINE auto root(T&& f) {
        return Detail::GetRoot<n>::value(std::forward<T>(f));
    }

    template <typename T>
    INLINE auto sqrt(T&& f) {
        return root<2>(std::forward<T>(f));
    }

    template <int d, typename = void>
    struct Pow {
        template <typename T>
        INLINE static auto value(T&& v) {
            return v * Pow<d - 1>::value(std::forward<T>(v));
        }
    };

    // clang-format off
    template <int d>
    struct Pow <d, std::enable_if_t<d<0>> {
        template<typename T>
        INLINE static auto value(T&& v) {
            return 1.f / Pow<-d>::value(std::forward<T>(v)); }
    };
    // clang-format on

    /// \todo maybe specialize first few pow-s to avoid recursion for small values of exponent?
    template <>
    struct Pow<0> {
        template <typename T>
        INLINE static auto value(T&& UNUSED(v)) {
            using RawT = std::decay_t<T>;
            return RawT(1.f);
        }
    };

    template <int n, typename T>
    INLINE auto pow(T&& v) {
        return Pow<n>::value(std::forward<T>(v));
    }

    template <typename T>
    INLINE T exp(const T f) {
        return ::exp(f);
    }

    template <typename T>
    INLINE T abs(const T f) {
        return ::fabs(f);
    }

    template <>
    INLINE int abs<int>(const int f) {
        return ::abs(f);
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

    /// \note One test failed because I rounded up Pi too much. I'm not taking any chances ;)
    constexpr Float PI = 3.14159265358979323846264338327950288419716939937510582097_f;

    constexpr Float PI_INV = 1._f / PI;

    constexpr Float E = 2.718281828459045235360287471352662497757247093699959574967_f;

    INLINE auto sphereVolume(const Float radius) {
        return 1.3333333333333333333333_f * PI * Math::pow<3>(radius);
    }

    /// Checks if two valeus are equal to some given accuracy.
    /// \note We use <= rather than < on purpose as EPS for integral types is zero.
    INLINE auto almostEqual(const Float& f1, const Float& f2, const Float& eps = EPS) {
        return abs(f1 - f2) <= eps;
    }

    /// Returns a norm of a object. Must be specialized to use other objects as quantities.
    template<typename T>
    INLINE Float norm(const T& value);

    /// Squared value of the norm.
    template<typename T>
    INLINE Float normSqr(const T& value);

    template<>
    INLINE Float norm(const Float& value) {
        return Math::abs(value);
    }
    template<>
    INLINE Float normSqr(const Float& value) {
        return Math::sqr(value);
    }

    template<typename T>
    INLINE bool isReal(const T& value) {
        return !std::isnan(value) && !std::isinf(value);
    }
}

NAMESPACE_SPH_END
