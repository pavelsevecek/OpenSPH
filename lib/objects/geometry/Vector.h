#pragma once

/// \file Vector.h
/// \brief Basic vector algebra. Computations are accelerated using SIMD.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Globals.h"
#include "objects/containers/Tuple.h"
#include "objects/geometry/Generic.h"
#include "objects/wrappers/Interval.h"
#include <immintrin.h>
#include <iomanip>
#include <smmintrin.h>

NAMESPACE_SPH_BEGIN

/// \brief Components of the 4D vector.
///
/// First 3 are simply cartesian coordinates, the 4th one is the smoothing length
enum Coordinate {
    X = 0,
    Y = 1,
    Z = 2,
    H = 3,
};


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

    /// Returns the data as SSE vector.
    INLINE const __m128& sse() const {
        return data;
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

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) {
        return !(v1 == v2);
    }

    friend std::ostream& operator<<(std::ostream& stream, const BasicVector& v) {
        constexpr int digits = 6;
        stream << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::setw(20) << v[i];
        }
        return stream;
    }
};


/// specialization for doubles or units of double precision

// #define SPH_VECTOR_AVX

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

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) {
        return !(v1 == v2);
    }

    INLINE auto dot(const BasicVector& other) const {
        /// \todo optimize
        return (*this)[X] * other[X] + (*this)[Y] * other[Y] + (*this)[Z] * other[Z];
    }

    // component-wise minimum
    INLINE BasicVector min(const BasicVector& other) const {
        return BasicVector(_mm256_min_pd(data, other.data));
    }

    // component-wise maximum
    INLINE BasicVector max(const BasicVector& other) const {
        return BasicVector(_mm256_max_pd(data, other.data));
    }

    INLINE const __m256d& sse() const {
        return data;
    }

    friend std::ostream& operator<<(std::ostream& stream, const BasicVector& v) {
        constexpr int digits = PRECISION;
        stream << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::setw(20) << v[i];
        }
        return stream;
    }
};

#else

// also align to 32 to have the same memory layout
template <>
class alignas(32) BasicVector<double> {
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

    template <int i>
    INLINE const __m128d& sse() const {
        static_assert(unsigned(i) < 2, "Invalid index");
        return data[i];
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

    INLINE friend bool operator!=(const BasicVector& v1, const BasicVector& v2) {
        return !(v1 == v2);
    }

    /// Output to stream
    friend std::ostream& operator<<(std::ostream& stream, const BasicVector& v) {
        constexpr int digits = PRECISION;
        stream << std::setprecision(digits);
        for (int i = 0; i < 3; ++i) {
            stream << std::setw(20) << v[i];
        }
        return stream;
    }
};

#endif

static_assert(alignof(BasicVector<double>) == 32, "Incorrect alignment of Vector");


using Vector = BasicVector<Float>;

/// Make sure the vector is trivially constructible and destructible, needed for fast initialization of arrays
static_assert(std::is_trivially_default_constructible<Vector>::value, "must be trivially construtible");
static_assert(std::is_trivially_destructible<Vector>::value, "must be trivially destructible");

/// Vector utils

/// Generic dot product between two vectors.
INLINE float dot(const BasicVector<float>& v1, const BasicVector<float>& v2) {
    constexpr int d = 3;
    return _mm_cvtss_f32(_mm_dp_ps(v1.sse(), v2.sse(), (1 << (d + 4)) - 0x0F));
}

INLINE double dot(const BasicVector<double>& v1, const BasicVector<double>& v2) {
    /// \todo optimize
    return v1[X] * v2[X] + v1[Y] * v2[Y] + v1[Z] * v2[Z];
}

/// Cross product between two vectors.
INLINE BasicVector<float> cross(const BasicVector<float>& v1, const BasicVector<float>& v2) {
    return _mm_sub_ps(_mm_mul_ps(_mm_shuffle_ps(v1.sse(), v1.sse(), _MM_SHUFFLE(3, 0, 2, 1)),
                          _mm_shuffle_ps(v2.sse(), v2.sse(), _MM_SHUFFLE(3, 1, 0, 2))),
        _mm_mul_ps(_mm_shuffle_ps(v1.sse(), v1.sse(), _MM_SHUFFLE(3, 1, 0, 2)),
            _mm_shuffle_ps(v2.sse(), v2.sse(), _MM_SHUFFLE(3, 0, 2, 1))));
}

INLINE BasicVector<double> cross(const BasicVector<double>& v1, const BasicVector<double>& v2) {
    /// \todo optimize
    return BasicVector<double>(
        v1[Y] * v2[Z] - v1[Z] * v2[Y], v1[Z] * v2[X] - v1[X] * v2[Z], v1[X] * v2[Y] - v1[Y] * v2[X]);
}

/// Returns squared length of the vector. Faster than simply getting length as there is no need to compute
/// square root.
INLINE Float getSqrLength(const Vector& v) {
    return dot(v, v);
}

/// Returns the length of the vector. Enabled only for vectors of floating-point precision.
INLINE Float getLength(const Vector& v) {
    return sqrt(dot(v, v));
}

/// Returns approximate value of the length. Enabled only for vectors of floating-point precision.
INLINE Float getLengthApprox(const Vector& v) {
    return sqrtApprox(dot(v, v));
}

/// Returns normalized vector. Throws assert if the vector has zero length. Enabled only for vectors of
/// floating-point precision.
INLINE Vector getNormalized(const Vector& v) {
    const Float length = getLength(v);
    ASSERT(length != 0._f);
    return v / length;
}

/// Returns normalized vector and length of the input vector as tuple.
INLINE Tuple<Vector, Float> getNormalizedWithLength(const Vector& v) {
    const Float length = getLength(v);
    ASSERT(length != 0._f);
    return { v / length, length };
}


/// Component-wise minimum
template <>
INLINE BasicVector<float> min(const BasicVector<float>& v1, const BasicVector<float>& v2) {
    return _mm_min_ps(v1.sse(), v2.sse());
}

/// Component-wise maximum
template <>
INLINE BasicVector<float> max(const BasicVector<float>& v1, const BasicVector<float>& v2) {
    return _mm_max_ps(v1.sse(), v2.sse());
}

#ifdef SPH_VECTOR_AVX
template <>
INLINE BasicVector<double> min(const BasicVector<double>& v1, const BasicVector<double>& v2) {
    return v1.min(v2);
}

INLINE BasicVector<double> max(const BasicVector<double>& v1, const BasicVector<double>& v2) {
    return v1.max(v2);
}
#else
template <>
INLINE BasicVector<double> min(const BasicVector<double>& v1, const BasicVector<double>& v2) {
    return { _mm_min_pd(v1.sse<0>(), v2.sse<0>()), _mm_min_pd(v1.sse<1>(), v2.sse<1>()) };
}

template <>
INLINE BasicVector<double> max(const BasicVector<double>& v1, const BasicVector<double>& v2) {
    return { _mm_max_pd(v1.sse<0>(), v2.sse<0>()), _mm_max_pd(v1.sse<1>(), v2.sse<1>()) };
}
#endif


/// Component-wise clamping
template <>
INLINE Vector clamp(const Vector& v, const Vector& v1, const Vector& v2) {
    return max(v1, min(v, v2));
}

/// Clamping all components by range.
template <>
INLINE Vector clamp(const Vector& v, const Interval& range) {
    return Vector(range.clamp(v[0]), range.clamp(v[1]), range.clamp(v[2]), range.clamp(v[3]));
}

/// Checks if two vectors are equal to some given accuracy.
INLINE bool almostEqual(const Vector& v1, const Vector& v2, const Float eps = EPS) {
    return getSqrLength(v1 - v2) <= sqr(eps) * (1._f + max(getSqrLength(v1), getSqrLength(v2)));
}

/// Returns norm of a vector, i.e. its (approximative) length
template <>
INLINE Float norm(const Vector& v) {
    const Float result = getLengthApprox(v);
    ASSERT(isReal(result));
    return result;
}

/// Returns squared length of a vector
template <>
INLINE Float normSqr(const Vector& v) {
    const Float result = getSqrLength(v);
    ASSERT(isReal(result));
    return result;
}

/// Returns minimum element of a vector. Considers only the first 3 component, 4th one is ignored.
template <>
INLINE Float minElement(const Vector& v) {
    /// \todo possibly optimize with SSE? Only 3 components though ...
    return min(v[0], v[1], v[2]);
}

/// Returns maximum element of a vector. Considers only the first 3 component, 4th one is ignored.
template <>
INLINE Float maxElement(const Vector& v) {
    return max(v[0], v[1], v[2]);
}

/// Returns the index of the minimum element.
INLINE Size argMin(const Vector& v) {
    Size minIdx = 0;
    if (v[1] < v[minIdx]) {
        minIdx = 1;
    }
    if (v[2] < v[minIdx]) {
        minIdx = 2;
    }
    return minIdx;
}

/// Returns the index of the maximum element.
INLINE Size argMax(const Vector& v) {
    Size maxIdx = 0;
    if (v[1] > v[maxIdx]) {
        maxIdx = 1;
    }
    if (v[2] > v[maxIdx]) {
        maxIdx = 2;
    }
    return maxIdx;
}

/// Computes vector of absolute values.
template <>
INLINE auto abs(const BasicVector<float>& v) {
    return BasicVector<float>(_mm_andnot_ps(_mm_set1_ps(-0.f), v.sse()));
}

#ifdef SPH_VECTOR_AVX
template <>
INLINE auto abs(const BasicVector<double>& v) {
    /// \todo optimize
    return BasicVector<double>(abs(v[X]), abs(v[Y]), abs(v[Z]), abs(v[H]));
}
#else
template <>
INLINE auto abs(const BasicVector<double>& v) {
    return BasicVector<double>(
        _mm_andnot_pd(_mm_set1_pd(-0.), v.sse<0>()), _mm_andnot_pd(_mm_set1_pd(-0.), v.sse<1>()));
}
#endif

/// Returns the L1 norm (sum of absolute values) of the vector
INLINE Float l1Norm(const Vector& v) {
    const Vector absV = abs(v);
    return absV[X] + absV[Y] + absV[Z];
}


/// Computes vector of inverse squared roots.
/*template <>
INLINE Vector sqrtInv(const Vector& v) {
    return _mm_rsqrt_ss(v.sse());
}*/

template <>
INLINE bool isReal(const BasicVector<float>& v) {
    /// \todo optimize using SSE intrinsics
    return isReal(v[0]) && isReal(v[1]) && isReal(v[2]);
}

template <>
INLINE bool isReal(const BasicVector<double>& v) {
    /// \todo optimize using SSE intrinsics
    return isReal(v[0]) && isReal(v[1]) && isReal(v[2]);
}

template <>
INLINE auto less(const Vector& v1, const Vector& v2) {
    /// \todo optimize
    return Vector(Float(v1[X] < v2[X]), Float(v1[Y] < v2[Y]), Float(v1[Z] < v2[Z]), Float(v1[H] < v2[H]));
}

template <>
INLINE StaticArray<Float, 6> getComponents<Vector>(const Vector& v) {
    return { v[X], v[Y], v[Z] };
}


/// Casts vector components to another precision. Casting vectors is only possible by using this function to
/// avoid problems with contructor/conversion operator ambiguity.
template <typename T1, typename T2>
INLINE BasicVector<T1> vectorCast(const BasicVector<T2>& v) {
    return BasicVector<T1>(v[X], v[Y], v[Z], v[H]);
}
template <>
INLINE BasicVector<Float> vectorCast(const BasicVector<Float>& v) {
    return v;
}

/// Cosine applied to all components of the vector.
INLINE Vector cos(const Vector& v) {
    /// \todo return _mm_cos_ps(v.sse());
    return Vector(cos(v[X]), cos(v[Y]), cos(v[Z]));
}

/// \brief Construct a vector from spherical coordinates.
///
/// The angle has generally different type to allow using units with dimensions.
/// \param r Radius coordinate
/// \param theta Latitude in radians, where 0 and PI correspond to poles.
/// \param phi Longitude in radians
INLINE Vector sphericalToCartesian(const Float r, const Float theta, const Float phi) {
    const Float s = sin(theta);
    const Float c = cos(theta);
    return r * Vector(s * cos(phi), s * sin(phi), c);
}

struct SphericalCoords {
    Float r;     ///< radius
    Float theta; ///< latitude
    Float phi;   ///< longitude
};

/// \brief Converts vector in cartesian coordinates to spherical coordinates
INLINE SphericalCoords cartensianToSpherical(const Vector& v) {
    const Float r = getLength(v);
    const Float phi = atan2(v[Y], v[X]);
    const Float theta = acos(v[Z] / r); // atan2(sqrt(sqr(v[X]) + sqr(v[Y])), v[Z]);
    return { r, theta, phi };
}


/// Computes a spherical inversion of a vector. Works in arbitrary number of dimensions.
/// \param v Vector to be inverted.
/// \param center Center of the spherical inversion.
/// \param radius Radius of the spherical inversion. For vectors in radius from center, spherical
/// inversion is
///               an identity transform.
INLINE Vector sphericalInversion(const Vector& v, const Vector& center, const Float radius) {
    const Vector diff = v - center;
    const Float lSqr = getSqrLength(diff);
    return center + diff * radius * radius / lSqr;
}

/// Returns the distance of vector from given axis. The axis is assumed to be normalized.
INLINE Float distance(const Vector& r, const Vector& axis) {
    ASSERT(almostEqual(getSqrLength(axis), 1._f));
    return getLength(r - dot(r, axis) * axis);
}

/// Compares components of two vectors lexicographically, primary component is z.
INLINE bool lexicographicalLess(const Vector& v1, const Vector& v2) {
    if (v1[Z] < v2[Z]) {
        return true;
    } else if (v1[Z] > v2[Z]) {
        return false;
    } else if (v1[Y] < v2[Y]) {
        return true;
    } else if (v1[Y] > v2[Y]) {
        return false;
    } else if (v1[X] < v2[X]) {
        return true;
    } else {
        return false;
    }
}

NAMESPACE_SPH_END
