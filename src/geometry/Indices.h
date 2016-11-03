#pragma once

/// Additional functionality for vector computations.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "objects/containers/ArrayView.h"
#include <functional>

NAMESPACE_SPH_BEGIN

template <typename T>
class DummyReferenceMaker : public Object {
private:
    T value;

public:
    DummyReferenceMaker() = default;

    DummyReferenceMaker(const T& value)
        : value(value) {}

    operator T&() { return value; }

    operator std::reference_wrapper<T>() { return std::ref(value); }
};

template <typename T1, typename T2>
struct IndexCast {
    template <typename T>
    static T1& cast(T&& value) {
        return static_cast<T1&>(value);
    }
};

template <typename T1>
struct IndexCast<T1, T1> {
    template <typename T>
    static T& cast(T&& value) {
        return value;
    }
};

template <typename T1, typename T2, typename T>
auto& indexCast(T&& value) {
    return IndexCast<T1, T2>::cast(std::forward<T>(value));
}

/// Helper object for storing three (possibly four) objects. Useful for computations in 3D space with other
/// objects than floating-point numbers (we have Vector for those). Can also store l-value references.
/// \todo specialize for non-ref types?
/// For reference types, the fourth component is still optional. If not used, however, getting the value will
/// access memory of destroyed temporary object and result in crash, the safety is left to the user.
template <typename T>
class BasicIndices : public Object {
    template <typename>
    friend class BasicIndices;

private:
    using RawT   = std::decay_t<T>;
    using StoreT = std::conditional_t<std::is_reference<T>::value, std::reference_wrapper<RawT>, RawT>;
    using ValueT = std::conditional_t<std::is_reference<T>::value, RawT&, const RawT&>;
    using DummyT = DummyReferenceMaker<RawT>;

    StoreT data[4];

public:
    BasicIndices() {}

    /// Constructs indices from values. Fourth component is optional.
    BasicIndices(ValueT i, ValueT j, ValueT k, ValueT l = DummyT())
        : data{ i, j, k, l } {}

    /// Copy constructor from const indices
    template <typename TOther = T, typename = std::enable_if_t<!std::is_reference<T>::value, TOther>>
    BasicIndices(const BasicIndices<TOther>& other) {
        //: data{ DummyT(), DummyT(), DummyT(), DummyT() } {
        for (int i = 0; i < 3; ++i) {
            data[i] = other.data[i];
        }
    }

    /// Copy constructor from l-value reference, enabled only for reference underlying type
    template <typename TOther = T, typename = std::enable_if_t<std::is_reference<T>::value, TOther>>
    BasicIndices(BasicIndices<TOther>& other)
        : data{ DummyT(), DummyT(), DummyT(), DummyT() } {
        // fill array with dummies, but override them immediately, so this is safe
        for (int i = 0; i < 3; ++i) {
            data[i] = other[i];
        }
    }

    template <typename TOther, typename = std::enable_if_t<std::is_reference<T>::value, TOther>>
    BasicIndices& operator=(BasicIndices<TOther>& other) {
        for (int i = 0; i < 3; ++i) {
            data[i] = other.data[i];
        }
        return *this;
    }

    template <typename TOther>
    BasicIndices& operator=(const BasicIndices<TOther>& other) {
        for (int i = 0; i < 3; ++i) {
            static_cast<T&>(data[i]) = other.data[i];
        }
        return *this;
    }

    RawT& operator[](const int idx) {
        ASSERT(unsigned(idx) < 4);
        return data[idx];
    }

    const RawT& operator[](const int idx) const {
        ASSERT(unsigned(idx) < 4);
        return data[idx];
    }

    bool operator==(const BasicIndices& other) {
        return data[0] == other.data[0] && data[1] == other.data[1] && data[2] == other.data[2];
    }

    bool operator!=(const BasicIndices& other) { return !(*this == other); }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const BasicIndices& indices) {
        stream << indices[0] << "  " << indices[1] << "  " << indices[2];
        return stream;
    }
};

template <typename T>
auto makeIndices(const T& i, const T& j, const T& k, const T& l = T()) {
    using RawT = std::decay_t<T>;
    return BasicIndices<RawT>(i, j, k, l);
}

template <typename T>
auto tieIndices(T& i, T& j, T& k, T& l = DummyReferenceMaker<std::remove_const_t<T>>()) {
    using RefT = std::remove_const_t<T>&;
    return BasicIndices<RefT>(i, j, k, l);
}

using Indices = BasicIndices<int>;

/// Returns a content of array of vectors, where each component is given by index.
INLINE auto getByMultiIndex(ArrayView<Vector> values, const Indices& idxs) {
    return tieIndices(values[idxs[0]][0], values[idxs[1]][1], values[idxs[2]][2]);
}

INLINE auto getByMultiIndex(ArrayView<Indices> values, const Indices& idxs) {
    return tieIndices(values[idxs[0]][0], values[idxs[1]][1], values[idxs[2]][2]);
}

NAMESPACE_SPH_END
