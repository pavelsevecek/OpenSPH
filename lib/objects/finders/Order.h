#pragma once

/// Helper object defining permutation.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Iterators.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

/// Permutation, i.e. (discrete) invertible function int->int. Simple wrapper of Array<int> with convenient
/// interface that guarantees the object will be always valid permutation. Only way to modify the object is
/// via <code>shuffle</code> function.
class Order : public Noncopyable {
private:
    Array<int> storage;


    /// Private constructor from int storage
    Order(Array<int>&& other)
        : storage(std::move(other)) {}

public:
    Order() = default;

    Order(Order&& other)
        : storage(std::move(other.storage)) {}

    /// Construct identity of given size
    Order(const int n)
        : storage(0, n) {
        for (int i = 0; i < n; ++i) {
            storage.push(i);
        }
    }

    Order& operator=(Order&& other) {
        storage = std::move(other.storage);
        return *this;
    }

    /// Shuffle order by given comparator
    template <typename TComparator>
    void shuffle(TComparator&& comparator) {
        std::sort(storage.begin(), storage.end(), std::forward<TComparator>(comparator));
    }

    /// Returns inverted order
    Order getInverted() const {
        Array<int> inverted(storage.size());
        for (int i = 0; i < storage.size(); ++i) {
            inverted[storage[i]] = i;
        }
        return inverted;
    }

    Order clone() const {
        return storage.clone();
    }

    /// Compose two orders
    Order operator()(const Order& other) const {
        Array<int> composed(storage.size());
        for (int i = 0; i < storage.size(); ++i) {
            composed[i] = storage[other[i]];
        }
        return composed;
    }

    INLINE int operator[](const int idx) const { return storage[idx]; }

    INLINE int size() const {
        return storage.size();
    }

    INLINE bool operator==(const Order& other) const { return storage == other.storage; }
};

/// Order in each component
class VectorOrder : public Object {
private:
    Array<StaticArray<int, 3>> storage;


    /// Private constructor from int storage
    VectorOrder(Array<StaticArray<int, 3>>&& other)
        : storage(std::move(other)) {}

public:
    /// Construct identity of given size
    VectorOrder(const int n)
        : storage(0, n) {
        for (int i = 0; i < n; ++i) {
            storage.push(StaticArray<int, 3>{i, i, i});
        }
    }

    /// Shuffle order by given comparator
    template <typename TComparator>
    void shuffle(const int component, TComparator&& comparator) {
        auto adapter = componentAdapter(storage, component);
        std::sort(adapter.begin(), adapter.end(), std::forward<TComparator>(comparator));
    }

    /// Returns inverted order
    VectorOrder getInverted() const {
        Array<StaticArray<int, 3>> inverted(storage.size());
        for (int i = 0; i < storage.size(); ++i) {
            for (int j = 0; j < 3; ++j) {
                inverted[storage[i][j]][j] = i;
            }
        }
        return inverted;
    }

    INLINE StaticArray<int, 3> operator[](const int idx) const { return storage[idx].clone(); }
};

NAMESPACE_SPH_END
