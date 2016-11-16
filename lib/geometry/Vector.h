#pragma once

/// Basic vector algebra. Computations are accelerated using SIMD.
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Globals.h"
#include "core/Traits.h"
#include "math/Math.h"
#include <immintrin.h>
#include <iomanip>
#include <smmintrin.h>


NAMESPACE_SPH_BEGIN

/// Components of the 4D vector. First 3 are simply cartesian coordinates,
/// the 4th one is the smoothing length
enum Coordinates { X = 0, Y = 1, Z = 2, H = 3 };


/// Helper type trait to determine if the type is a vector of some kind
template <typename T>
struct IsVectorType {
    static constexpr bool value = false;
};

template <typename T>
class BasicVector;

template <typename T>
struct IsVectorType<BasicVector<T>> {
    static constexpr bool value = true;
};

template <typename T>
constexpr bool IsVector = IsVectorType<T>::value;

// test
static_assert(!IsVector<float>, "static test failed");
static_assert(IsVector<BasicVector<float>>, "Static test failed");


template <typename T>
class BasicVector;

/// 3-dimensional vector, float precision
template <>
class BasicVector<float> : public Object {
private:
    __m128 data;

public:
    INLINE BasicVector() = default;

    /// Constructs from SSE vector.
    INLINE BasicVector(const __m128 data)
        : data(data) {}

    /// Constructs by copying a value to all vector components
    INLINE explicit BasicVector(const float f)
        : data(_mm_set1_ps(float(f))) {}

    /// Constructs the vector from given components.
    INLINE BasicVector(const float x, const float y, const float z = 0.f, const float h = 0.f)
        : data(_mm_set_ps(h, z, y, x)) {}

    /// Copy constructor
    INLINE BasicVector(const BasicVector& v)
        : data(v.data) {}

    /// Move constructor
    INLINE BasicVector(BasicVector&& v)
        : data(std::move(v.data)) {}

    /// Get component by given index.
    INLINE const float& operator[](const int i) const { return *(reinterpret_cast<const float*>(&data) + i); }

    /// Get component by given index.
    INLINE float& operator[](const int i) { return *(reinterpret_cast<float*>(&data) + i); }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE const float& get() const {
        return *(reinterpret_cast<const float*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE float& get() {
        return *(reinterpret_cast<float*>(&data) + i);
    }

    /// Copy operator
    INLINE BasicVector& operator=(const BasicVector& v) {
        data = v.data;
        return *this;
    }

    /// Move operator
    INLINE BasicVector& operator=(BasicVector&& v) {
        data = std::move(v.data);
        return *this;
    }

    INLINE BasicVector& operator+=(const BasicVector& v) {
        data = _mm_add_ps(data, v.data);
        return *this;
    }

    INLINE BasicVector& operator-=(const BasicVector& v) {
        data = _mm_sub_ps(data, v.data);
        return *this;
    }

    INLINE BasicVector& operator*=(const float f) {
        data = _mm_mul_ps(data, _mm_set1_ps(f));
        return *this;
    }

    INLINE BasicVector& operator/=(const float f) {
        data = _mm_div_ps(data, _mm_set1_ps(f));
        return *this;
    }

    INLINE BasicVector& operator*=(const BasicVector& v) {
        data = _mm_mul_ps(data, v.data);
        return *this;
    }

    INLINE BasicVector& operator/=(const BasicVector& v) {
        data = _mm_div_ps(data, v.data);
        return *this;
    }

    INLINE friend BasicVector operator-(const BasicVector& v) {
        return _mm_xor_ps(v.data, _mm_set1_ps(-0.f));
    }

    INLINE friend BasicVector operator+(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_add_ps(v1.data, v2.data));
    }

    INLINE friend BasicVector operator-(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_sub_ps(v1.data, v2.data));
    }

    /// Multiplication of vector by a value or unit
    INLINE friend auto operator*(const BasicVector& v, const float f) {
        return BasicVector(_mm_mul_ps(v.data, _mm_set1_ps(f)));
    }

    INLINE friend auto operator*(const float f, const BasicVector& v) {
        return BasicVector(_mm_mul_ps(v.data, _mm_set1_ps(f)));
    }

    INLINE friend auto operator*(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_mul_ps(v1.data, v2.data));
    }

    INLINE friend auto operator/(const BasicVector& v, const float f) {
        ASSERT(f != 0.f);
        return BasicVector(_mm_div_ps(v.data, _mm_set1_ps(f)));
    }

    INLINE friend auto operator/(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_div_ps(v1.data, v2.data));
    }

    /// Comparison operator, only enabled for vectors of same type.
    INLINE friend bool operator==(const BasicVector& v1, const BasicVector& v2) {
        constexpr int d = 3;
        const int mask  = (1 << d) - 0x01;
        return (_mm_movemask_ps(_mm_cmpeq_ps(v1.data, v2.data)) & mask) == mask;
    }

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) { return !(v1 == v2); }

    INLINE auto dot(const BasicVector& other) const {
        constexpr int d = 3;
        return _mm_cvtss_f32(_mm_dp_ps(data, other.data, (1 << (d + 4)) - 0x0F));
    }

    INLINE auto cross(const BasicVector& other) const {
        return BasicVector(
            _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 2, 1)),
                                  _mm_shuffle_ps(other.data, other.data, _MM_SHUFFLE(3, 1, 0, 2))),
                       _mm_mul_ps(_mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 1, 0, 2)),
                                  _mm_shuffle_ps(other.data, other.data, _MM_SHUFFLE(3, 0, 2, 1)))));
    }

    /// Component-wise minimum
    INLINE BasicVector min(const BasicVector& other) const {
        return BasicVector(_mm_min_ps(data, other.data));
    }

    /// Component-wise maximum
    INLINE BasicVector max(const BasicVector& other) const {
        return BasicVector(_mm_max_ps(data, other.data));
    }

    /// Output to stream
    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const BasicVector& v) {
        constexpr int digits = 6;
        stream << std::fixed << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::fixed << std::setprecision(digits) << v[i] << (i < 2 ? "  " : "");
        }
        return stream;
    }
};

/// specialization for doubles or units of double precision
/// TODO: make AVX variant as well for comparison

#define SPH_VECTOR_AVX

#ifdef SPH_VECTOR_AVX
template <>
class BasicVector<double> : public Object {
private:
    __m256d data;

public:
    INLINE BasicVector() = default;

    /// Constructs from SSE vector.
    INLINE BasicVector(const __m256d data)
        : data(data) {}

    /// Constructs by copying a value to all vector components
    INLINE explicit BasicVector(const double f)
        : data(_mm256_set1_pd(f)) {}

    /// Constructs the vector from given components.
    INLINE BasicVector(const double x, const double y, const double z = 0., const double h = 0.)
        : data(_mm256_set_pd(h, z, y, x)) {}

    /// Copy constructor
    INLINE BasicVector(const BasicVector& v)
        : data(v.data) {}

    /// Move constructor
    INLINE BasicVector(BasicVector&& v)
        : data(std::move(v.data)) {}

    /// Get component by given index.
    INLINE const double& operator[](const int i) const {
        return *(reinterpret_cast<const double*>(&data) + i);
    }

    /// Get component by given index.
    INLINE double& operator[](const int i) { return *(reinterpret_cast<double*>(&data) + i); }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE const double& get() const {
        return *(reinterpret_cast<const double*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE double& get() {
        return *(reinterpret_cast<double*>(&data) + i);
    }

    /// Copy operator
    INLINE BasicVector& operator=(const BasicVector& v) {
        data = v.data;
        return *this;
    }

    /// Move operator
    INLINE BasicVector& operator=(BasicVector&& v) {
        data = std::move(v.data);
        return *this;
    }

    INLINE BasicVector& operator+=(const BasicVector& v) {
        data = _mm256_add_pd(data, v.data);
        return *this;
    }

    INLINE BasicVector& operator-=(const BasicVector& v) {
        data = _mm256_sub_pd(data, v.data);
        return *this;
    }

    INLINE BasicVector& operator*=(const double f) {
        data = _mm256_mul_pd(data, _mm256_set1_pd(f));
        return *this;
    }

    INLINE BasicVector& operator/=(const double f) {
        data = _mm256_div_pd(data, _mm256_set1_pd(f));
        return *this;
    }

    INLINE BasicVector& operator*=(const BasicVector& v) {
        data = _mm256_mul_pd(data, v.data);
        return *this;
    }

    INLINE BasicVector& operator/=(const BasicVector& v) {
        data = _mm256_div_pd(data, v.data);
        return *this;
    }

    INLINE friend BasicVector operator-(const BasicVector& v) {
        return _mm256_xor_pd(v.data, _mm256_set1_pd(-0.));
    }

    INLINE friend BasicVector operator+(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm256_add_pd(v1.data, v2.data));
    }

    INLINE friend BasicVector operator-(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm256_sub_pd(v1.data, v2.data));
    }

    /// Multiplication of vector by a value or unit
    INLINE friend auto operator*(const BasicVector& v, const double f) {
        return BasicVector(_mm256_mul_pd(v.data, _mm256_set1_pd(f)));
    }

    INLINE friend auto operator*(const double f, const BasicVector& v) {
        return BasicVector(_mm256_mul_pd(v.data, _mm256_set1_pd(f)));
    }

    INLINE friend auto operator*(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm256_mul_pd(v1.data, v2.data));
    }

    INLINE friend auto operator/(const BasicVector& v, const double f) {
        ASSERT(f != 0.);
        return BasicVector(_mm256_div_pd(v.data, _mm256_set1_pd(f)));
    }

    INLINE friend auto operator/(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm256_div_pd(v1.data, v2.data));
    }

    /// Comparison operator, only enabled for vectors of same type.
    INLINE friend bool operator==(const BasicVector& v1, const BasicVector& v2) {
        /// \todo optimize
        return v1[X] == v2[X] && v1[Y] == v2[Y] && v1[Z] == v2[Z];
    }

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) { return !(v1 == v2); }

    INLINE auto dot(const BasicVector& other) const {
        /// \todo optimize
        return (*this)[X] * other[X] + (*this)[Y] * other[Y] + (*this)[Z] * other[Z];
    }

    INLINE auto cross(const BasicVector& other) const {
        return BasicVector(
            _mm256_sub_pd(_mm256_mul_pd(_mm256_shuffle_pd(data, data, _MM_SHUFFLE(3, 0, 2, 1)),
                                        _mm256_shuffle_pd(other.data, other.data, _MM_SHUFFLE(3, 1, 0, 2))),
                          _mm256_mul_pd(_mm256_shuffle_pd(data, data, _MM_SHUFFLE(3, 1, 0, 2)),
                                        _mm256_shuffle_pd(other.data, other.data, _MM_SHUFFLE(3, 0, 2, 1)))));
    }

    // component-wise minimum
    INLINE BasicVector min(const BasicVector& other) const {
        return BasicVector(_mm256_min_pd(data, other.data));
    }

    // component-wise maximum
    INLINE BasicVector max(const BasicVector& other) const {
        return BasicVector(_mm256_max_pd(data, other.data));
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const BasicVector& v) {
        constexpr int digits = 12;
        stream << std::fixed << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::fixed << std::setprecision(digits) << v[i] << (i < 2 ? "  " : "");
        }
        return stream;
    }
};

using Vector = BasicVector<Float>;

#else
template <typename TUnit, int d>
class Vector<TUnit, d, typename std::enable_if_t<std::is_same<Base<TUnit>, double>::value, void>>
    : public VectorBase<Vector<TUnit, d>, TUnit, d> {
private:
    __m128d data[2];

public:
    Vector() = default;

    explicit Vector(const __m128d sse1, const __m128d sse2)
        : data{ sse1, sse2 } {}

    // copy unit to all vector components, assuming a conversion TUnit -> double exists
    explicit Vector(const TUnit f)
        : data{ _mm_set1_pd(double(f)), _mm_set1_pd(double(f)) } {}

    Vector(const TUnit x, const TUnit y, const TUnit z, const TUnit h)
        : data{ _mm_set_pd(double(y), double(x)), _mm_set_pd(double(h), double(z)) } {}

    // TODO enable only if d==3
    Vector(const TUnit x, const TUnit y, const TUnit z)
        : data{ _mm_set_pd(double(y), double(x)), _mm_set_pd(0., double(z)) } {}

    // TODO enable only if d==2
    Vector(const TUnit x, const TUnit y)
        : data{ _mm_set_pd(double(y), double(x)), _mm_set1_pd(0.) } {}


    Vector(const Vector& v)
        : data{ v.data[0], v.data[1] } {}

    INLINE TUnit get(const int i) const { return TUnit(*(((double*)data) + i)); }

    INLINE TUnit& get(const int i) { return *reinterpret_cast<TUnit*>(((double*)data) + i); }

    void operator+=(const Vector& v) {
        data[0] = _mm_add_pd(data[0], v.data[0]);
        data[1] = _mm_add_pd(data[1], v.data[1]);
    }
    void operator-=(const Vector& v) {
        data[0] = _mm_sub_pd(data[0], v.data[0]);
        data[1] = _mm_sub_pd(data[1], v.data[1]);
    }
    void operator*=(const TUnit f) {
        const __m128d sse = _mm_set1_pd(double(f));
        data[0]           = _mm_mul_pd(data[0], sse);
        data[1]           = _mm_mul_pd(data[1], sse);
    }
    void operator/=(const TUnit f) {
        const __m128d sse = _mm_set1_pd(double(f));
        data[0]           = _mm_div_pd(data[0], sse);
        data[1]           = _mm_div_pd(data[1], sse);
    }

    INLINE Vector friend operator+(const Vector& v1, const Vector& v2) {
        return Vector(_mm_add_pd(v1.data[0], v2.data[0]), _mm_add_pd(v1.data[1], v2.data[1]));
    }

    INLINE Vector friend operator-(const Vector& v1, const Vector& v2) {
        return Vector(_mm_sub_pd(v1.data[0], v2.data[0]), _mm_sub_pd(v1.data[1], v2.data[1]));
    }

    INLINE Vector friend operator*(const Vector& v, const TUnit f) {
        const __m128d sse = _mm_set1_pd(double(f));
        return Vector(_mm_mul_pd(v.data[0], sse), _mm_mul_pd(v.data[1], sse));
    }

    INLINE Vector friend operator*(const TUnit f, const Vector& v) { return v * f; }

    INLINE Vector friend operator/(const Vector& v, const TUnit f) {
        ASSERT(f != 0);
        const __m128d sse = _mm_set1_pd(double(f));
        return Vector(_mm_div_pd(v.data[0], sse), _mm_div_pd(v.data[1], sse));
    }

    Vector friend operator/(const Vector& v1, const Vector& v2) {
        return Vector(_mm_div_pd(v1.data[0], v2.data[0]), _mm_div_pd(v1.data[1], v2.data[1]));
    }

    bool friend operator==(const Vector& v1, const Vector& v2) {
        const int mask1 = (1 << d) - 0x01;
        const int mask2 = (1 << (d - 2)) - 0x01;
        return (_mm_movemask_pd(_mm_cmpeq_pd(v1.data[0], v2.data[0])) & mask1) == mask1 &&
               (_mm_movemask_pd(_mm_cmpeq_pd(v1.data[1], v2.data[1])) & mask2) == mask2;
    }

    // TODO:
    static TUnit dot(const Vector& v1, const Vector& v2) {
        switch (d) {
        case 2:
            return v1[X] * v2[X] + v1[Y] * v2[Y];
        case 3:
            return v1[X] * v2[X] + v1[Y] * v2[Y] + v1[Z] * v2[Z];
        }
    }
    /*
            static Vector3f cross(const Vector3f& v1, const Vector3f& v2) {
                return Vector3f(_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(v1.data, v1.data, _MM_SHUFFLE(3, 0, 2,
       1)),
                                                      _mm_shuffle_ps(v2.data, v2.data, _MM_SHUFFLE(3, 1, 0,
       2))),
                                           _mm_mul_ps(_mm_shuffle_ps(v1.data, v1.data, _MM_SHUFFLE(3, 1, 0,
       2)),
                                                      _mm_shuffle_ps(v2.data, v2.data, _MM_SHUFFLE(3, 0, 2,
           1)))));
            }
    */
    // component-wise minimum
    static Vector min(const Vector& v1, const Vector& v2) {
        return Vector(_mm_min_pd(v1.data[0], v2.data[0]), _mm_min_pd(v1.data[1], v2.data[1]));
    }

    // component-wise maximum
    static Vector max(const Vector& v1, const Vector& v2) {
        return Vector(_mm_max_pd(v1.data[0], v2.data[0]), _mm_max_pd(v1.data[1], v2.data[1]));
    }
};
#endif

/// Printing components to stream


/// Vector utils

/// Generic dot product between two vectors.
INLINE auto dot(const Vector& v1, const Vector& v2) {
    return v1.dot(v2);
}

/// Cross product between two vectors.
INLINE auto cross(const Vector& v1, const Vector& v2) {
    return v1.cross(v2);
}

/// Returns squared length of the vector. Faster than simply getting length as there is no need to compute
/// square root.
INLINE auto getSqrLength(const Vector& v) {
    return dot(v, v);
}

/// Returns the length of the vector. Enabled only for vectors of floating-point precision.
INLINE auto getLength(const Vector& v) {
    return Math::sqrt(dot(v, v));
}

/// Returns approximate value of the length. Enabled only for vectors of floating-point precision.
INLINE auto getLengthApprox(const Vector& v) {
    return Math::sqrtApprox(dot(v, v));
}

/// Returns normalized vector. Throws assert if the vector has zero length. Enabled only for vectors of
/// floating-point precision.
INLINE auto getNormalized(const Vector& v) {
    const Float length = getLength(v);
    ASSERT(length != 0._f);
    return v / getLength(v);
}

namespace Math {
    /// Component-wise minimum
    INLINE Vector min(const Vector& v1, const Vector& v2) { return v1.min(v2); }

    /// Component-wise maximum
    INLINE Vector max(const Vector& v1, const Vector& v2) { return v1.max(v2); }

    /// Component-wise clamping
    INLINE Vector clamp(const Vector& v, const Vector& v1, const Vector& v2) { return max(v1, min(v, v2)); }

    /// Checks if two vectors are equal to some given accuracy.
    INLINE bool almostEqual(const Vector& v1, const Vector& v2, const Float eps = EPS) {
        return getSqrLength(v1 - v2) <= Math::sqr(eps);
    }

    INLINE Float norm(const Vector& v) { return getLengthApprox(v); }

    INLINE Float normSqr(const Vector& v) { return getSqrLength(v); }

    INLINE bool isReal(const Vector& v) {
        /// \todo optimize using SSE intrinsics
        return isReal(v[0]) && isReal(v[1]) && isReal(v[2]);
    }

    /// Cosine applied to all components of the vector.
    INLINE BasicVector<float> cos(const BasicVector<float>& v) {
        /// \todo optimize
        return BasicVector<float>(cos(v[0]), cos(v[1]), cos(v[2]), cos(v[3]));
    }
}

/// Construct a vector from spherical coordinates. The angle has generally different
/// type to allow using units with dimensions.
/// \param r Radius coordinate
/// \param theta Latitude in radians, where 0 and PI correspond to poles.
/// \param phi Longitude in radians
INLINE Vector spherical(const Float r, const Float theta, const Float phi) {
    const Float s = Math::sin(theta);
    const Float c = Math::cos(theta);
    return r * Vector(s * Math::cos(phi), s * Math::sin(phi), c);
}

/// Computes a spherical inversion of a vector. Works in arbitrary number of dimensions.
/// \param v Vector to be inverted.
/// \param center Center of the spherical inversion.
/// \param radius Radius of the spherical inversion. For vectors in radius from center, spherical inversion is
///               an identity transform.
INLINE Vector sphericalInversion(const Vector& v, const Vector& center, const Float radius) {
    const Vector diff = v - center;
    const Float lSqr  = getSqrLength(diff);
    return center + diff * radius * radius / lSqr;
}

NAMESPACE_SPH_END
