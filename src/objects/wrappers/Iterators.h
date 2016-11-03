#pragma once

/// Iterator adapters.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"

NAMESPACE_SPH_BEGIN

/// Provides iterator over selected component of vector array.
template <typename TIterator>
struct ComponentIterator : public Object {
    TIterator iterator;
    int component;

    using T = decltype((*std::declval<TIterator>())[std::declval<int>()]);
    using RawT = std::decay_t<T>;

    ComponentIterator() = default;

    ComponentIterator(const TIterator& iterator, const int component)
        : iterator(iterator)
        , component(component) {}

    const RawT& operator*() const { return (*iterator)[component]; }

    RawT& operator*() { return (*iterator)[component]; }

    ComponentIterator& operator++() {
        ++iterator;
        return *this;
    }
    ComponentIterator operator++(int) {
        ComponentIterator tmp(*this);
        operator++();
        return tmp;
    }
    ComponentIterator& operator--() {
        --iterator;
        return *this;
    }
    ComponentIterator operator--(int) {
        ComponentIterator tmp(*this);
        operator--();
        return tmp;
    }


    ComponentIterator operator+(const int n) const { return ComponentIterator(iterator+n, component); }
    ComponentIterator operator-(const int n) const { return ComponentIterator(iterator-n, component); }
    void operator+=(const int n) { iterator += n; }
    void operator-=(const int n) { iterator -= n; }

    size_t operator-(const ComponentIterator& other) const { return iterator - other.iterator; }
    bool operator<(const ComponentIterator& other) const { return iterator < other.iterator; }
    bool operator>(const ComponentIterator& other) const { return iterator > other.iterator; }
    bool operator<=(const ComponentIterator& other) const { return iterator <= other.iterator; }
    bool operator>=(const ComponentIterator& other) const { return iterator >= other.iterator; }
    bool operator==(const ComponentIterator& other) { return iterator == other.iterator; }
    bool operator!=(const ComponentIterator& other) { return iterator != other.iterator; }


    using iterator_category = std::random_access_iterator_tag;
    using value_type        = RawT;
    using difference_type   = size_t;
    using pointer           = RawT*;
    using reference         = RawT&;

};


/// Wrapper for iterating over given component of vector array
template <typename TBuffer>
struct VectorComponentAdapter : public Object {
    TBuffer data;
    const int component;

    using TIterator = decltype(std::declval<TBuffer>().begin());

    VectorComponentAdapter(TBuffer&& data, const int component)
        : data(std::forward<TBuffer>(data))
        , component(component) {}

    auto begin() { return ComponentIterator<TIterator>(data.begin(), component); }

    auto end() { return ComponentIterator<TIterator>(data.end(), component); }
};


template <typename TBuffer>
auto componentAdapter(TBuffer&& buffer, const int component) {
    return VectorComponentAdapter<TBuffer>(std::forward<TBuffer>(buffer), component);
}

NAMESPACE_SPH_END
