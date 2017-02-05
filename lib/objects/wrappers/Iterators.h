#pragma once

/// Iterator adapters.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"

NAMESPACE_SPH_BEGIN


template <typename TContainer>
class ReverseWrapper {
private:
    TContainer container;

public:
    template <typename T>
    ReverseWrapper(T&& container)
        : container(std::forward<T>(container)) {
    }

    /// Returns iterator pointing to the last element in container.
    auto begin() {
        return reverseIterator(container.end() - 1);
    }

    /// Returns iterator pointiing to one before the first element.
    auto end() {
        return reverseIterator(container.begin() - 1);
    }

    auto size() const {
        return container.size();
    }
};

template <typename TContainer>
ReverseWrapper<TContainer> reverse(TContainer&& container) {
    return ReverseWrapper<TContainer>(std::forward<TContainer>(container));
}


/// Provides iterator over selected component of vector array.
template <typename TIterator>
struct ComponentIterator {
    TIterator iterator;
    int component;

    using T = decltype((*std::declval<TIterator>())[std::declval<Size>()]);
    using RawT = std::decay_t<T>;

    ComponentIterator() = default;

    ComponentIterator(const TIterator& iterator, const int component)
        : iterator(iterator)
        , component(component) {
    }

    const RawT& operator*() const {
        return (*iterator)[component];
    }

    RawT& operator*() {
        return (*iterator)[component];
    }

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


    ComponentIterator operator+(const int n) const {
        return ComponentIterator(iterator + n, component);
    }
    ComponentIterator operator-(const int n) const {
        return ComponentIterator(iterator - n, component);
    }
    void operator+=(const int n) {
        iterator += n;
    }
    void operator-=(const int n) {
        iterator -= n;
    }

    Size operator-(const ComponentIterator& other) const {
        return iterator - other.iterator;
    }
    bool operator<(const ComponentIterator& other) const {
        return iterator < other.iterator;
    }
    bool operator>(const ComponentIterator& other) const {
        return iterator > other.iterator;
    }
    bool operator<=(const ComponentIterator& other) const {
        return iterator <= other.iterator;
    }
    bool operator>=(const ComponentIterator& other) const {
        return iterator >= other.iterator;
    }
    bool operator==(const ComponentIterator& other) {
        return iterator == other.iterator;
    }
    bool operator!=(const ComponentIterator& other) {
        return iterator != other.iterator;
    }


    using iterator_category = std::random_access_iterator_tag;
    using value_type = RawT;
    using difference_type = Size;
    using pointer = RawT*;
    using reference = RawT&;
};


/// Wrapper for iterating over given component of vector array
template <typename TBuffer>
struct VectorComponentAdapter {
    TBuffer data;
    const int component;

    using TIterator = decltype(std::declval<TBuffer>().begin());

    VectorComponentAdapter(TBuffer&& data, const int component)
        : data(std::forward<TBuffer>(data))
        , component(component) {
    }

    auto begin() {
        return ComponentIterator<TIterator>(data.begin(), component);
    }

    auto end() {
        return ComponentIterator<TIterator>(data.end(), component);
    }
};


template <typename TBuffer>
auto componentAdapter(TBuffer&& buffer, const int component) {
    return VectorComponentAdapter<TBuffer>(std::forward<TBuffer>(buffer), component);
}

NAMESPACE_SPH_END
