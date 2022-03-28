#pragma once

/// \file Indices.h
/// \brief Vectorized computations with integral numbers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/Object.h"
#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

#define SPH_NO_ROUNDING_MODE

/// Helper object for storing three (possibly four) int or bool values.

#ifdef SPH_ARM

class Indices {
private:
    int data[4];

public:
    INLINE Indices() = default;

    /// Constructs indices from single value by copying it to all components.
    INLINE explicit Indices(const int value)
        : data{ value, value, value, value } {}

    /// Constructs indices from values. Fourth component is optional.
    INLINE Indices(const int i, const int j, const int k, const int l = 0)
        : data{ i, j, k, l } {}

/// Constructs indices by casting components of vectors to ints
#ifndef SPH_NO_ROUNDING_MODE
    INLINE explicit Indices(const BasicVector<float>& v) {
        data = _mm_cvtps_epi32(v.sse());
    }
#else
    INLINE explicit Indices(const BasicVector<float>& v)
        : Indices(int(v[X]), int(v[Y]), int(v[Z]), int(v[H])) {}
#endif

    INLINE explicit Indices(const BasicVector<double>& v) {
        /// \todo optimize
        *this = Indices(int(v[0]), int(v[1]), int(v[2]), int(v[3]));
    }

    INLINE Indices(const Indices& other) = default;

    /// Must be called once before Indices are used
    INLINE static void init() {
#ifndef SPH_NO_ROUNDING_MODE
        _MM_SET_ROUNDING_MODE(_MM_ROUND_DOWN);
#endif
    }

    INLINE Indices& operator=(const Indices& other) = default;

#ifndef SPH_NO_ROUNDING_MODE
    INLINE operator BasicVector<float>() const {
        return BasicVector<float>(_mm_cvtepi32_ps(data));
    }
#else
    INLINE operator BasicVector<float>() const {
        return BasicVector<float>(float((*this)[0]), float((*this)[1]), float((*this)[2]), float((*this)[3]));
    }
#endif

    INLINE operator BasicVector<double>() const {
        /// \todo optimize
        return BasicVector<double>((*this)[0], (*this)[1], (*this)[2], (*this)[3]);
    }

    INLINE int& operator[](const int idx) {
        SPH_ASSERT(unsigned(idx) < 4);
        return data[idx];
    }

    INLINE int operator[](const int idx) const {
        SPH_ASSERT(unsigned(idx) < 4);
        return data[idx];
    }

    INLINE Indices operator==(const Indices& other) const {
        return Indices(data[0] == other[0], data[1] == other[1], data[2] == other[2], data[3] == other[3]);
    }

    INLINE Indices operator!=(const Indices& other) {
        return Indices(data[0] != other[0], data[1] != other[1], data[2] != other[2], data[3] != other[3]);
    }

    INLINE Indices operator>(const Indices& other) const {
        return Indices(data[0] > other[0], data[1] > other[1], data[2] > other[2], data[3] > other[3]);
    }

    INLINE Indices operator<(const Indices& other) const {
        return Indices(data[0] < other[0], data[1] < other[1], data[2] < other[2], data[3] < other[3]);
    }

    INLINE Indices operator+(const Indices& other) const {
        return Indices(data[0] + other[0], data[1] + other[1], data[2] + other[2], data[3] + other[3]);
    }

    INLINE Indices operator-(const Indices& other) const {
        return Indices(data[0] - other[0], data[1] - other[1], data[2] - other[2], data[3] - other[3]);
    }

    INLINE Indices max(const Indices& other) const {
        return Indices(Sph::max(data[0], other[0]),
            Sph::max(data[1], other[1]),
            Sph::max(data[2], other[2]),
            Sph::max(data[3], other[3]));
    }

    INLINE Indices min(const Indices& other) const {
        return Indices(Sph::min(data[0], other[0]),
            Sph::min(data[1], other[1]),
            Sph::min(data[2], other[2]),
            Sph::min(data[3], other[3]));
    }

    friend std::ostream& operator<<(std::ostream& stream, const Indices& idxs) {
        for (int i = 0; i < 3; ++i) {
            stream << std::setw(20) << idxs[i];
        }
        return stream;
    }
};

#else
class Indices {
private:
    union {
        __m128i i;
        int a[4];
    } data;


public:
    INLINE Indices() = default;

    INLINE Indices(__m128i i) {
        data.i = i;
    }

    /// Constructs indices from single value by copying it to all components.
    INLINE explicit Indices(const int value) {
        data.i = _mm_set1_epi32(value);
    }

    /// Constructs indices from values. Fourth component is optional.
    INLINE Indices(const int i, const int j, const int k, const int l = 0) {
        data.i = _mm_set_epi32(l, k, j, i);
    }

/// Constructs indices by casting components of vectors to ints
#ifndef SPH_NO_ROUNDING_MODE
    INLINE explicit Indices(const BasicVector<float>& v) {
        data = _mm_cvtps_epi32(v.sse());
    }
#else
    INLINE explicit Indices(const BasicVector<float>& v)
        : Indices(int(v[X]), int(v[Y]), int(v[Z]), int(v[H])) {}
#endif

    INLINE explicit Indices(const BasicVector<double>& v) {
        /// \todo optimize
        *this = Indices(int(v[0]), int(v[1]), int(v[2]), int(v[3]));
    }

    INLINE Indices(const Indices& other)
        : data(other.data) {}

    /// Must be called once before Indices are used
    INLINE static void init() {
#ifndef SPH_NO_ROUNDING_MODE
        _MM_SET_ROUNDING_MODE(_MM_ROUND_DOWN);
#endif
    }

    INLINE Indices& operator=(const Indices& other) {
        data = other.data;
        return *this;
    }

#ifndef SPH_NO_ROUNDING_MODE
    INLINE operator BasicVector<float>() const {
        return BasicVector<float>(_mm_cvtepi32_ps(data));
    }
#else
    INLINE operator BasicVector<float>() const {
        return BasicVector<float>(float((*this)[0]), float((*this)[1]), float((*this)[2]), float((*this)[3]));
    }
#endif

    INLINE operator BasicVector<double>() const {
        /// \todo optimize
        return BasicVector<double>((*this)[0], (*this)[1], (*this)[2], (*this)[3]);
    }

    INLINE int& operator[](const int idx) {
        SPH_ASSERT(unsigned(idx) < 4);
        return data.a[idx];
    }

    INLINE int operator[](const int idx) const {
        SPH_ASSERT(unsigned(idx) < 4);
        return data.a[idx];
    }

    INLINE Indices operator==(const Indices& other) const {
        return _mm_cmpeq_epi32(data.i, other.data.i);
    }

    INLINE Indices operator!=(const Indices& other) {
        return _mm_xor_si128(_mm_cmpeq_epi32(data.i, other.data.i), _mm_set1_epi32(-1));
    }

    INLINE Indices operator>(const Indices& other) const {
        return _mm_cmpgt_epi32(data.i, other.data.i);
    }

    INLINE Indices operator<(const Indices& other) const {
        return _mm_cmplt_epi32(data.i, other.data.i);
    }

    INLINE Indices operator+(const Indices& other) const {
        return _mm_add_epi32(data.i, other.data.i);
    }

    INLINE Indices operator-(const Indices& other) const {
        return _mm_sub_epi32(data.i, other.data.i);
    }

    INLINE Indices max(const Indices& other) const {
        return _mm_max_epi32(data.i, other.data.i);
    }

    INLINE Indices min(const Indices& other) const {
        return _mm_min_epi32(data.i, other.data.i);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Indices& idxs) {
        for (int i = 0; i < 3; ++i) {
            stream << std::setw(20) << idxs[i];
        }
        return stream;
    }
};

#endif

INLINE Indices max(const Indices i1, const Indices i2) {
    return i1.max(i2);
}

INLINE Indices min(const Indices i1, const Indices i2) {
    return i1.min(i2);
}

INLINE bool all(const Indices& i) {
    return i[0] && i[1] && i[2];
}

INLINE bool any(const Indices& i) {
    return i[0] || i[1] || i[2];
}

template <>
INLINE auto floor(const Vector& v) {
    return Indices(int(std::floor(v[X])), int(std::floor(v[Y])), int(std::floor(v[Z])));
}

struct IndicesEqual {
    INLINE bool operator()(const Indices& i1, const Indices& i2) const {
        return all(i1 == i2);
    }
};

NAMESPACE_SPH_END

template <>
class std::hash<Sph::Indices> {
public:
    INLINE size_t operator()(const Sph::Indices& idxs) const {
        return (idxs[0] * 73856093ull) ^ (idxs[1] * 19349663ull) ^ (idxs[2] * 83492791ull);
    }
};
