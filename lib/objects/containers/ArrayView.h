#pragma once

/// Base classes useful for iterating and viewing all containers.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"
#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// Simple (forward) iterator. Can be used with STL algorithms.
template <typename T, typename TCounter = int>
class Iterator : public Object {
    template <typename, typename>
    friend class Array;
    template <typename, int>
    friend class StaticArray;
    template <typename, typename>
    friend class ArrayView;

protected:
    using TValue = typename UnwrapReferenceType<T>::Type;

    T* data;
    const T *begin, *end;

    Iterator(T* data, const T* begin, const T* end)
        : data(data)
        , begin(begin)
        , end(end) {}


public:
    Iterator() = default;

    const TValue& operator*() const {
        ASSERT(data >= begin);
        ASSERT(data <= end);
        return *data;
    }
    TValue& operator*() {
        ASSERT(data >= begin);
        ASSERT(data <= end);
        return *data;
    }
    Iterator operator+(const TCounter n) const { return Iterator(data + n, begin, end); }
    Iterator operator-(const TCounter n) const { return Iterator(data - n, begin, end); }
    void operator+=(const TCounter n) { data += n; }
    void operator-=(const TCounter n) { data -= n; }
    Iterator& operator++() {
        (*this) += 1;
        return *this;
    }
    Iterator operator++(int) {
        Iterator tmp(*this);
        operator++();
        return tmp;
    }
    Iterator& operator--() {
        (*this) -= 1;
        return *this;
    }
    Iterator operator--(int) {
        Iterator tmp(*this);
        operator--();
        return tmp;
    }
    size_t operator-(const Iterator& iter) const { return data - iter.data; }
    bool operator<(const Iterator& iter) const { return data < iter.data; }
    bool operator>(const Iterator& iter) const { return data > iter.data; }
    bool operator<=(const Iterator& iter) const { return data <= iter.data; }
    bool operator>=(const Iterator& iter) const { return data >= iter.data; }
    bool operator==(const Iterator& iter) const { return data == iter.data; }
    bool operator!=(const Iterator& iter) const { return data != iter.data; }


    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = size_t;
    using pointer           = T*;
    using reference         = T&;
};


/// Object providing safe access to continuous memory of data, useful to write generic code that can be used
/// with any kind of storage where the data are stored consecutively in memory.
template <typename T, typename TCounter = int>
class ArrayView : public Object {
private:
    using StorageType = typename WrapReferenceType<T>::Type;

    StorageType* data;
    TCounter actSize = 0;

public:
    ArrayView()
        : data(nullptr) {}

    ArrayView(StorageType* data, TCounter size)
        : data(data)
        , actSize(size) {}

    explicit ArrayView(std::initializer_list<StorageType> list)
        : data(&*list.begin())
        , actSize(list.size()) {}

    ArrayView(const ArrayView& other)
        : data(other.data)
        , actSize(other.actSize) {}

    /// Move constructor
    ArrayView(ArrayView&& other) {
        std::swap(this->data, other.data);
        std::swap(this->actSize, other.actSize);
    }

    /// Copy operator
    ArrayView& operator=(const ArrayView& other) {
        this->data    = other.data;
        this->actSize = other.actSize;
        return *this;
    }

    /// Move operator
    ArrayView& operator=(ArrayView&& other) {
        std::swap(this->data, other.data);
        std::swap(this->actSize, other.actSize);
        return *this;
    }

    /// Implicit conversion to const version
    operator ArrayView<const T, TCounter>() { return ArrayView<const T, TCounter>(data, actSize); }

    Iterator<StorageType, TCounter> begin() { return Iterator<StorageType>(data, data, data + actSize); }

    Iterator<const StorageType, TCounter> begin() const {
        return Iterator<const StorageType>(data, data, data + actSize);
    }

    Iterator<StorageType, TCounter> end() {
        return Iterator<StorageType>(data + actSize, data, data + actSize);
    }

    Iterator<const StorageType, TCounter> end() const {
        return Iterator<StorageType>(data + actSize, data, data + actSize);
    }

    INLINE T& operator[](const TCounter idx) {
        ASSERT(unsigned(idx) < unsigned(actSize));
        return data[idx];
    }

    INLINE const T& operator[](const TCounter idx) const {
        ASSERT(unsigned(idx) < unsigned(actSize));
        return data[idx];
    }

    INLINE int size() const { return this->actSize; }

    INLINE bool empty() const { return this->actSize == 0; }

    bool operator!() const { return data == nullptr; }
};


NAMESPACE_SPH_END
