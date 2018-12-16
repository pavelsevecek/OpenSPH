#pragma once

/// \file Order.h
/// \brief Helper object defining permutation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/Array.h"
#include "objects/geometry/Indices.h"
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

    /// \brief Construct identity of given size
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

    /// \brief Shuffles the order using a binary predicate.
    template <typename TBinaryPredicate>
    void shuffle(TBinaryPredicate&& predicate) {
        std::sort(storage.begin(), storage.end(), predicate);
    }

    /// \brief Returns the inverted order
    Order getInverted() const {
        Array<Size> inverted(storage.size());
        for (Size i = 0; i < storage.size(); ++i) {
            inverted[storage[i]] = i;
        }
        return inverted;
    }

    /// \brief Clones the order.
    Order clone() const {
        return storage.clone();
    }

    /// \brief Composes two orders
    Order compose(const Order& other) const {
        Array<Size> composed(storage.size());
        for (Size i = 0; i < storage.size(); ++i) {
            composed[i] = storage[other[i]];
        }
        return composed;
    }

    /// \brief Shuffles given array using this order.
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

/// \brief Finds the order of values in given array.
///
/// The returned order, when applied on sorted values, gives the original (unsorted) values
INLINE Order getOrder(ArrayView<const Float> values) {
    Order order(values.size());
    order.shuffle([values](Size i, Size j) { return values[i] < values[j]; });
    return order.getInverted();
}


NAMESPACE_SPH_END
