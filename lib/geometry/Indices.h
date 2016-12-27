#pragma once

/// Additional functionality for vector computations.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "objects/containers/ArrayView.h"
#include <functional>

NAMESPACE_SPH_BEGIN

/// Helper object for storing three (possibly four) int or bool values.
class Indices : public Object {
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
    INLINE explicit Indices(const Vector& v) {
        /// \todo without rounding mode?
        _MM_SET_ROUNDING_MODE(_MM_ROUND_DOWN);
        data = _mm_cvtps_epi32(v.data);
    }

    INLINE Indices(const Indices& other)
        : data(other.data) {}

    INLINE Indices& operator=(const Indices& other) {
        data = other.data;
        return *this;
    }

    INLINE operator Vector() const { return Vector(_mm_cvtepi32_ps(data)); }

    INLINE int& operator[](const int idx) {
        ASSERT(unsigned(idx) < 4);
        return *(reinterpret_cast<int*>(&data) + idx);
    }

    INLINE int operator[](const int idx) const {
        ASSERT(unsigned(idx) < 4);
        return *(reinterpret_cast<const int*>(&data) + idx);
    }

    INLINE Indices operator==(const Indices& other) const { return _mm_cmpeq_epi32(data, other.data); }

    INLINE Indices operator!=(const Indices& other) {
        return _mm_xor_si128(_mm_cmpeq_epi32(data, other.data), _mm_set1_epi32(-1));
    }

    INLINE Indices operator>(const Indices& other) const { return _mm_cmpgt_epi32(data, other.data); }

    INLINE Indices operator<(const Indices& other) const { return _mm_cmplt_epi32(data, other.data); }

    INLINE Indices operator+(const Indices& other) const { return _mm_add_epi32(data, other.data); }

    INLINE Indices operator-(const Indices& other) const { return _mm_sub_epi32(data, other.data); }

    INLINE Indices max(const Indices& other) const { return _mm_max_epi32(data, other.data); }

    INLINE Indices min(const Indices& other) const { return _mm_min_epi32(data, other.data); }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Indices& indices) {
        stream << indices[0] << "  " << indices[1] << "  " << indices[2];
        return stream;
    }
};

namespace Math {
    INLINE Indices max(const Indices i1, const Indices i2) { return i1.max(i2); }

    INLINE Indices min(const Indices i1, const Indices i2) { return i1.min(i2); }
}

/*/// Returns a content of array of vectors, where each component is given by index.
INLINE auto getByMultiIndex(ArrayView<Vector> values, const Indices& idxs) {
    return tieIndices(values[idxs[0]][0], values[idxs[1]][1], values[idxs[2]][2]);
}

INLINE auto getByMultiIndex(ArrayView<Indices> values, const Indices& idxs) {
    return tieIndices(values[idxs[0]][0], values[idxs[1]][1], values[idxs[2]][2]);
}*/

NAMESPACE_SPH_END
