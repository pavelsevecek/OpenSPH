#pragma once

/// Iterator adapters.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"
#include <iterator>

NAMESPACE_SPH_BEGIN

/// Simple (forward) iterator. Can be used with STL algorithms.
template <typename T, typename TCounter = Size>
class Iterator {
    template <typename, typename>
    friend class Array;
    template <typename, int, typename>
    friend class StaticArray;
    template <typename, typename>
    friend class ArrayView;

protected:
    using TValue = typename UnwrapReferenceType<T>::Type;

    T* data;
#ifdef DEBUG
    const T *begin, *end;
#endif

#ifndef DEBUG
    Iterator(T* data)
        : data(data) {}
#endif

    Iterator(T* data, const T* UNUSED_IN_RELEASE(begin), const T* UNUSED_IN_RELEASE(end))
        : data(data)
#ifdef DEBUG
        , begin(begin)
        , end(end)
#endif
    {
    }


public:
    Iterator() = default;

    const TValue& operator*() const {
        ASSERT(data >= begin);
        ASSERT(data < end);
        return *data;
    }
    TValue& operator*() {
        ASSERT(data >= begin);
        ASSERT(data < end);
        return *data;
    }
#ifdef DEBUG
    Iterator operator+(const TCounter n) const {
        return Iterator(data + n, begin, end);
    }
    Iterator operator-(const TCounter n) const {
        return Iterator(data - n, begin, end);
    }
#else
    Iterator operator+(const TCounter n) const {
        return Iterator(data + n);
    }
    Iterator operator-(const TCounter n) const {
        return Iterator(data - n);
    }
#endif
    void operator+=(const TCounter n) {
        data += n;
    }
    void operator-=(const TCounter n) {
        data -= n;
    }
    Iterator& operator++() {
        ++data;
        return *this;
    }
    Iterator operator++(int) {
        Iterator tmp(*this);
        operator++();
        return tmp;
    }
    Iterator& operator--() {
        --data;
        return *this;
    }
    Iterator operator--(int) {
        Iterator tmp(*this);
        operator--();
        return tmp;
    }
    Size operator-(const Iterator& iter) const {
        ASSERT(data >= iter.data);
        return data - iter.data;
    }
    bool operator<(const Iterator& iter) const {
        return data < iter.data;
    }
    bool operator>(const Iterator& iter) const {
        return data > iter.data;
    }
    bool operator<=(const Iterator& iter) const {
        return data <= iter.data;
    }
    bool operator>=(const Iterator& iter) const {
        return data >= iter.data;
    }
    bool operator==(const Iterator& iter) const {
        return data == iter.data;
    }
    bool operator!=(const Iterator& iter) const {
        return data != iter.data;
    }


    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = Size;
    using pointer = T*;
    using reference = T&;
};

/// Reverse iterator.
template <typename T, typename TCounter = Size>
class ReverseIterator {
protected:
    Iterator<T, Size> iter;

public:
    ReverseIterator() = default;

    ReverseIterator(Iterator<T, Size> iter)
        : iter(iter) {}

    decltype(auto) operator*() const {
        return *iter;
    }
    decltype(auto) operator*() {
        return *iter;
    }
    ReverseIterator& operator++() {
        --iter;
        return *this;
    }
    ReverseIterator operator++(int) {
        ReverseIterator tmp(*this);
        operator++();
        return tmp;
    }
    ReverseIterator& operator--() {
        ++iter;
        return *this;
    }
    ReverseIterator operator--(int) {
        ReverseIterator tmp(*this);
        operator--();
        return tmp;
    }
    bool operator==(const ReverseIterator& other) const {
        return iter == other.iter;
    }
    bool operator!=(const ReverseIterator& other) const {
        return iter != other.iter;
    }
};

template <typename T, typename TCounter>
ReverseIterator<T, TCounter> reverseIterator(const Iterator<T, TCounter> iter) {
    return ReverseIterator<T, TCounter>(iter);
}

template <typename TContainer>
class ReverseWrapper {
private:
    TContainer container;

public:
    template <typename T>
    ReverseWrapper(T&& container)
        : container(std::forward<T>(container)) {}

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
        , component(component) {}

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
        , component(component) {}

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
