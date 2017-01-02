#pragma once

/// Basic vector algebra. Computations are accelerated using SIMD.
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Globals.h"
#include "core/Traits.h"
#include "objects/containers/Tuple.h"
#include "objects/wrappers/Range.h"
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
class BasicVector<float> {
    friend class Indices;

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
    INLINE BasicVector(const float x, const float y, const float z, const float h = 0.f)
        : data(_mm_set_ps(h, z, y, x)) {}

    /// Copy constructor
    INLINE BasicVector(const BasicVector& v)
        : data(v.data) {}

    /// Get component by given index.
    INLINE const float& operator[](const int i) const {
        ASSERT(unsigned(i) < 4);
        return *(reinterpret_cast<const float*>(&data) + i);
    }

    /// Get component by given index.
    INLINE float& operator[](const int i) {
        ASSERT(unsigned(i) < 4);
        return *(reinterpret_cast<float*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE const float& get() const {
        static_assert(unsigned(i) < 4, "Invalid index");
        return *(reinterpret_cast<const float*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE float& get() {
        static_assert(unsigned(i) < 4, "Invalid index");
        return *(reinterpret_cast<float*>(&data) + i);
    }

    /// Copy operator
    INLINE BasicVector& operator=(const BasicVector& v) {
        data = v.data;
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

    /// Comparison operator, only compares first three components of vectors
    INLINE friend bool operator==(const BasicVector& v1, const BasicVector& v2) {
        constexpr int d = 3;
        const int mask = (1 << d) - 0x01;
        return (_mm_movemask_ps(_mm_cmpeq_ps(v1.data, v2.data)) & mask) == mask;
    }

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) { return !(v1 == v2); }

    INLINE auto dot(const BasicVector& other) const {
        constexpr int d = 3;
        return _mm_cvtss_f32(_mm_dp_ps(data, other.data, (1 << (d + 4)) - 0x0F));
    }

    INLINE auto cross(const BasicVector& other) const {
        return BasicVector(_mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 2, 1)),
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
};


/// specialization for doubles or units of double precision

//#define SPH_VECTOR_AVX

#ifdef SPH_VECTOR_AVX
template <>
class BasicVector<double> {
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

    /// Get component by given index.
    INLINE const double& operator[](const int i) const {
        ASSERT(unsigned(i) < 4);
        return *(reinterpret_cast<const double*>(&data) + i);
    }

    /// Get component by given index.
    INLINE double& operator[](const int i) {
        ASSERT(unsigned(i) < 4);
        return *(reinterpret_cast<double*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE const double& get() const {
        static_assert(unsigned(i) < 4, "Invalid index");
        return *(reinterpret_cast<const double*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE double& get() {
        static_assert(unsigned(i) < 4, "Invalid index");
        return *(reinterpret_cast<double*>(&data) + i);
    }

    /// Copy operator
    INLINE BasicVector& operator=(const BasicVector& v) {
        data = v.data;
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

    /// Comparison operator, only compares first three components of vectors
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
    void toStream(TStream& stream) const {
        constexpr int digits = 12;
        stream << std::fixed << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::fixed << std::setprecision(digits) << (*this)[i];
        }
    }
};

#else

template <>
class BasicVector<double> {

private:
    __m128d data[2];

public:
    INLINE BasicVector() = default;

    /// Constructs from two SSE vectors.
    INLINE BasicVector(const __m128d data1, const __m128d data2)
        : data{ data1, data2 } {}

    /// Constructs by copying a value to all vector components
    INLINE explicit BasicVector(const double f)
        : data{ _mm_set1_pd(f), _mm_set1_pd(f) } {}

    /// Constructs the vector from given components.
    INLINE BasicVector(const double x, const double y, const double z, const double h = 0.)
        : data{ _mm_set_pd(y, x), _mm_set_pd(h, z) } {}

    /// Copy constructor
    INLINE BasicVector(const BasicVector& v)
        : data{ v.data[0], v.data[1] } {}

    /// Get component by given index.
    INLINE const double& operator[](const int i) const {
        ASSERT(unsigned(i) < 4);
        return *(reinterpret_cast<const double*>(&data) + i);
    }

    /// Get component by given index.
    INLINE double& operator[](const int i) {
        ASSERT(unsigned(i) < 4);
        return *(reinterpret_cast<double*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE const double& get() const {
        static_assert(unsigned(i) < 4, "Invalid index");
        return *(reinterpret_cast<const double*>(&data) + i);
    }

    /// Get component by given compile-time constant index
    template <int i>
    INLINE double& get() {
        static_assert(unsigned(i) < 4, "Invalid index");
        return *(reinterpret_cast<double*>(&data) + i);
    }

    /// Copy operator
    INLINE BasicVector& operator=(const BasicVector& v) {
        data[0] = v.data[0];
        data[1] = v.data[1];
        return *this;
    }

    INLINE BasicVector& operator+=(const BasicVector& v) {
        data[0] = _mm_add_pd(data[0], v.data[0]);
        data[1] = _mm_add_pd(data[1], v.data[1]);
        return *this;
    }

    INLINE BasicVector& operator-=(const BasicVector& v) {
        data[0] = _mm_sub_pd(data[0], v.data[0]);
        data[1] = _mm_sub_pd(data[1], v.data[1]);
        return *this;
    }

    INLINE BasicVector& operator*=(const double f) {
        const __m128d value = _mm_set1_pd(f);
        data[0] = _mm_mul_pd(data[0], value);
        data[1] = _mm_mul_pd(data[1], value);
        return *this;
    }

    INLINE BasicVector& operator/=(const double f) {
        const __m128d value = _mm_set1_pd(f);
        data[0] = _mm_div_pd(data[0], value);
        data[1] = _mm_div_pd(data[1], value);
        return *this;
    }

    INLINE BasicVector& operator*=(const BasicVector& v) {
        data[0] = _mm_mul_pd(data[0], v.data[0]);
        data[1] = _mm_mul_pd(data[1], v.data[1]);
        return *this;
    }

    INLINE BasicVector& operator/=(const BasicVector& v) {
        data[0] = _mm_div_pd(data[0], v.data[0]);
        data[1] = _mm_div_pd(data[1], v.data[1]);
        return *this;
    }

    INLINE friend BasicVector operator-(const BasicVector& v) {
        const __m128d value = _mm_set1_pd(-0.);
        return { _mm_xor_pd(v.data[0], value), _mm_xor_pd(v.data[1], value) };
    }

    INLINE friend BasicVector operator+(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_add_pd(v1.data[0], v2.data[0]), _mm_add_pd(v1.data[1], v2.data[1]));
    }

    INLINE friend BasicVector operator-(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_sub_pd(v1.data[0], v2.data[0]), _mm_sub_pd(v1.data[1], v2.data[1]));
    }

    /// Multiplication of vector by a value or unit
    INLINE friend auto operator*(const BasicVector& v, const double f) {
        const __m128d value = _mm_set1_pd(f);
        return BasicVector(_mm_mul_pd(v.data[0], value), _mm_mul_pd(v.data[1], value));
    }

    INLINE friend auto operator*(const double f, const BasicVector& v) {
        const __m128d value = _mm_set1_pd(f);
        return BasicVector(_mm_mul_pd(v.data[0], value), _mm_mul_pd(v.data[1], value));
    }

    INLINE friend auto operator*(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_mul_pd(v1.data[0], v2.data[0]), _mm_mul_pd(v1.data[1], v2.data[1]));
    }

    INLINE friend auto operator/(const BasicVector& v, const double f) {
        ASSERT(f != 0.);
        const __m128d value = _mm_set1_pd(f);
        return BasicVector(_mm_div_pd(v.data[0], value), _mm_div_pd(v.data[1], value));
    }

    INLINE friend auto operator/(const BasicVector& v1, const BasicVector& v2) {
        return BasicVector(_mm_div_pd(v1.data[0], v2.data[0]), _mm_div_pd(v1.data[1], v2.data[1]));
    }

    /// Comparison operator, only compares first three components of vectors
    INLINE friend bool operator==(const BasicVector& v1, const BasicVector& v2) {
        constexpr int d = 3;
        const int r1 = _mm_movemask_pd(_mm_cmpeq_pd(v1.data[0], v2.data[0]));
        const int r2 = _mm_movemask_pd(_mm_cmpeq_pd(v1.data[1], v2.data[1]));
        const int mask1 = (1 << 2) - 0x01;
        const int mask2 = (1 << (d - 2)) - 0x01;
        return (r1 & mask1) == mask1 && (r2 & mask2) == mask2;
    }

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) { return !(v1 == v2); }

    INLINE auto dot(const BasicVector& other) const {
        /// \todo optimize
        return this->get<0>() * other.get<0>() + this->get<1>() * other.get<1>() +
               this->get<2>() * other.get<2>();
        /*const __m128d first  = _mm_dp_pd(data[0], other.data[0], 0x31);
        const __m128d second = _mm_dp_pd(data[1], other.data[1], 0x21);
        return *(const double*)(&first) + *(const double*)(&second);*/
    }

    /// \todo optimize
    INLINE auto cross(const BasicVector& other) const {
        return BasicVector(this->get<1>() * other.get<2>() - this->get<2>() * other.get<1>(),
            this->get<2>() * other.get<0>() - this->get<0>() * other.get<2>(),
            this->get<0>() * other.get<1>() - this->get<1>() * other.get<0>());
    }

    /// Component-wise minimum
    INLINE BasicVector min(const BasicVector& other) const {
        return BasicVector(_mm_min_pd(data[0], other.data[0]), _mm_min_pd(data[1], other.data[1]));
    }

    /// Component-wise maximum
    INLINE BasicVector max(const BasicVector& other) const {
        return BasicVector(_mm_max_pd(data[0], other.data[0]), _mm_max_pd(data[1], other.data[1]));
    }

    /// Output to stream
    template <typename TStream>
    void toStream(TStream& stream) const {
        constexpr int digits = 12;
        stream << std::fixed << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::fixed << std::setprecision(digits) << (*this)[i];
        }
    }
};
#endif


using Vector = BasicVector<Float>;


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
    return v / length;
}

/// Returns normalized vector and length of the input vector as tuple.
INLINE Tuple<Vector, Float> getNormalizedWithLength(const Vector& v) {
    const Float length = getLength(v);
    ASSERT(length != 0._f);
    return makeTuple(v / length, length);
}

namespace Math {
    /// Component-wise minimum
    INLINE Vector min(const Vector& v1, const Vector& v2) { return v1.min(v2); }

    /// Component-wise maximum
    INLINE Vector max(const Vector& v1, const Vector& v2) { return v1.max(v2); }

    /// Component-wise clamping
    INLINE Vector clamp(const Vector& v, const Vector& v1, const Vector& v2) { return max(v1, min(v, v2)); }

    /// Clamping all components by range.
    INLINE Vector clamp(const Vector& v, const Range& range) {
        return Vector(range.clamp(v[0]), range.clamp(v[1]), range.clamp(v[2]), range.clamp(v[3]));
    }

    /// Checks if two vectors are equal to some given accuracy.
    INLINE bool almostEqual(const Vector& v1, const Vector& v2, const Float eps = EPS) {
        return getSqrLength(v1 - v2) <= Math::sqr(eps);
    }

    INLINE Float norm(const Vector& v) {
        const Float result = getLengthApprox(v);
        ASSERT(Math::isReal(result));
        return result;
    }

    INLINE Float normSqr(const Vector& v) {
        const Float result = getSqrLength(v);
        ASSERT(Math::isReal(result));
        return result;
    }

    INLINE bool isReal(const Vector& v) {
        /// \todo optimize using SSE intrinsics
        return isReal(v[0]) && isReal(v[1]) && isReal(v[2]);
    }

    /// Cosine applied to all components of the vector.
    INLINE Vector cos(const Vector& v) {
        /// \todo optimize
        return Vector(cos(v[0]), cos(v[1]), cos(v[2]), cos(v[3]));
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
    const Float lSqr = getSqrLength(diff);
    return center + diff * radius * radius / lSqr;
}

NAMESPACE_SPH_END

namespace std {
    /// Converts vector to string
    INLINE string to_string(const Sph::Vector& v) {
        constexpr int digits = 6;
        stringstream ss;
        ss << fixed << setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            ss << setw(15) << fixed << setprecision(digits) << v[i];
        }
        return ss.str();
    }
}
