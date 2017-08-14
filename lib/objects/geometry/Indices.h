#pragma once

/// \file Indices.h
/// \brief Vectorized computations with integral numbers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/geometry/Vector.h"
#include "objects/Object.h"
#include <functional>

NAMESPACE_SPH_BEGIN

// #define NO_ROUNDING_MODE


/// Helper object for storing three (possibly four) int or bool values.
class Indices {
private:
    __m128i data;

public:
    INLINE Indices() = default;

    INLINE Indices(__m128i data)
        : data(data) {}

    /// Constructs indices from single value by copying it to all components.
    INLINE explicit Indices(const int value)
        : data(_mm_set1_epi32(value)) {}

    /// Constructs indices from values. Fourth component is optional.
    INLINE Indices(const int i, const int j, const int k, const int l = 0)
        : data(_mm_set_epi32(l, k, j, i)) {}

/// Constructs indices by casting components of vectors to ints
#ifndef NO_ROUNDING_MODE
    INLINE explicit Indices(const BasicVector<float>& v) {
        data = _mm_cvtps_epi32(v.sse());
    }
#else
    INLINE explicit Indices(const BasicVector<float>& v)
        : Indices(int(v[X]), int(v[Y]), int(v[Z]), int(v[H])) {}
#endif

    INLINE explicit Indices(const BasicVector<double>& v) {
        /// \todo optimize
        *this = Indices(v[0], v[1], v[2], v[3]);
    }

    INLINE Indices(const Indices& other)
        : data(other.data) {}

    /// Must be called once before Indices are used
    INLINE static void init() {
#ifndef NO_ROUNDING_MODE
        _MM_SET_ROUNDING_MODE(_MM_ROUND_DOWN);
#endif
    }

    INLINE Indices& operator=(const Indices& other) {
        data = other.data;
        return *this;
    }

#ifndef NO_ROUNDING_MODE
    INLINE operator BasicVector<float>() const {
        return BasicVector<float>(_mm_cvtepi32_ps(data));
    }
#else
    INLINE operator BasicVector<float>() const {
        return BasicVector<float>((*this)[0], (*this)[1], (*this)[2], (*this)[3]);
    }
#endif

    INLINE operator BasicVector<double>() const {
        /// \todo optimize
        return BasicVector<double>((*this)[0], (*this)[1], (*this)[2], (*this)[3]);
    }

    INLINE int& operator[](const int idx) {
        ASSERT(unsigned(idx) < 4);
        return *(reinterpret_cast<int*>(&data) + idx);
    }

    INLINE int operator[](const int idx) const {
        ASSERT(unsigned(idx) < 4);
        return *(reinterpret_cast<const int*>(&data) + idx);
    }

    INLINE Indices operator==(const Indices& other) const {
        return _mm_cmpeq_epi32(data, other.data);
    }

    INLINE Indices operator!=(const Indices& other) {
        return _mm_xor_si128(_mm_cmpeq_epi32(data, other.data), _mm_set1_epi32(-1));
    }

    INLINE Indices operator>(const Indices& other) const {
        return _mm_cmpgt_epi32(data, other.data);
    }

    INLINE Indices operator<(const Indices& other) const {
        return _mm_cmplt_epi32(data, other.data);
    }

    INLINE Indices operator+(const Indices& other) const {
        return _mm_add_epi32(data, other.data);
    }

    INLINE Indices operator-(const Indices& other) const {
        return _mm_sub_epi32(data, other.data);
    }

    INLINE Indices max(const Indices& other) const {
        return _mm_max_epi32(data, other.data);
    }

    INLINE Indices min(const Indices& other) const {
        return _mm_min_epi32(data, other.data);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Indices& idxs) {
        for (int i = 0; i < 3; ++i) {
            stream << std::setw(20) << idxs[i];
        }
        return stream;
    }
};

INLINE Indices max(const Indices i1, const Indices i2) {
    return i1.max(i2);
}

INLINE Indices min(const Indices i1, const Indices i2) {
    return i1.min(i2);
}

INLINE bool all(const Indices& i) {
    return i[0] && i[1] && i[2];
}

/*/// Returns a content of array of vectors, where each component is given by index.
INLINE auto getByMultiIndex(ArrayView<Vector> values, const Indices& idxs) {
    return tieIndices(values[idxs[0]][0], values[idxs[1]][1], values[idxs[2]][2]);
}

INLINE auto getByMultiIndex(ArrayView<Indices> values, const Indices& idxs) {
    return tieIndices(values[idxs[0]][0], values[idxs[1]][1], values[idxs[2]][2]);
}*/

NAMESPACE_SPH_END
