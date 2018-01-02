#pragma once

/// \file Order.h
/// \brief Helper object defining permutation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/Array.h"
#include "objects/geometry/Indices.h"
#include "objects/utility/Iterators.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

/// \brief Permutation, i.e. (discrete) invertible function int->int.
///
/// Simple wrapper of Array<size_t> with convenient interface that guarantees the object will be always valid
/// permutation. Only way to modify the object is via <code>shuffle</code> function.
class Order : public Noncopyable {
private:
    Array<Size> storage;


    /// Private constructor from size_t storage
    Order(Array<Size>&& other)
        : storage(std::move(other)) {}

public:
    Order() = default;

    Order(Order&& other)
        : storage(std::move(other.storage)) {}

    /// Construct identity of given size
    explicit Order(const Size n)
        : storage(0, n) {
        for (Size i = 0; i < n; ++i) {
            storage.push(i);
        }
    }

    Order& operator=(Order&& other) {
        storage = std::move(other.storage);
        return *this;
    }

    /// Shuffle order by given binary predicate.
    template <typename TBinaryPredicate>
    void shuffle(TBinaryPredicate&& predicate) {
        std::sort(storage.begin(), storage.end(), predicate);
    }

    /// Returns inverted order
    Order getInverted() const {
        Array<Size> inverted(storage.size());
        for (Size i = 0; i < storage.size(); ++i) {
            inverted[storage[i]] = i;
        }
        return inverted;
    }

    Order clone() const {
        return storage.clone();
    }

    /// Compose two orders
    Order compose(const Order& other) const {
        Array<Size> composed(storage.size());
        for (Size i = 0; i < storage.size(); ++i) {
            composed[i] = storage[other[i]];
        }
        return composed;
    }

    /// Shuffles given array using this order
    template <typename T>
    Array<T> apply(const Array<T>& input) {
        Array<T> sorted(input.size());
        for (Size i = 0; i < input.size(); ++i) {
            sorted[i] = input[storage[i]];
        }
        return sorted;
    }

    INLINE Size operator[](const Size idx) const {
        return storage[idx];
    }

    INLINE Size size() const {
        return storage.size();
    }

    INLINE bool operator==(const Order& other) const {
        return storage == other.storage;
    }
};

/// Order in each component
class VectorOrder : public Noncopyable {
private:
    Array<Indices> storage;


    /// Private constructor from int storage
    VectorOrder(Array<Indices>&& other)
        : storage(std::move(other)) {}

public:
    VectorOrder();

    VectorOrder(VectorOrder&& other)
        : storage(std::move(other.storage)) {}

    /// Construct identity of given size
    VectorOrder(const Size n)
        : storage(0, n) {
        for (Size i = 0; i < n; ++i) {
            storage.push(Indices(i));
        }
    }

    VectorOrder& operator=(VectorOrder&& other) {
        storage = std::move(other.storage);
        return *this;
    }

    /// Shuffle order by given comparator
    template <typename TComparator>
    void shuffle(const uint component, TComparator&& comparator) {
        auto adapter = componentAdapter(storage, component);
        std::sort(adapter.begin(), adapter.end(), std::forward<TComparator>(comparator));
    }

    /// Returns inverted order
    VectorOrder getInverted() const {
        Array<Indices> inverted(storage.size());
        for (Size i = 0; i < storage.size(); ++i) {
            for (uint j = 0; j < 3; ++j) {
                inverted[storage[i][j]][j] = i;
            }
        }
        return inverted;
    }

    INLINE const Indices& operator[](const Size idx) const {
        return storage[idx];
    }
};

NAMESPACE_SPH_END
