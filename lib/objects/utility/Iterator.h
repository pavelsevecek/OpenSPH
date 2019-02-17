#pragma once

/// \file Iterator.h
/// \brief Ordinary iterator for custom containers.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Assert.h"
#include "common/Traits.h"
#include <iterator>

NAMESPACE_SPH_BEGIN

/// \brief Simple (forward) iterator over continuous array of objects of type T.
///
/// Can be used with STL algorithms.
template <typename T, typename TCounter = Size>
class Iterator {
protected:
    using TValue = typename UnwrapReferenceType<T>::Type;

    T* data;
#ifdef SPH_DEBUG
    const T *begin = nullptr, *end = nullptr;
#endif

#ifndef SPH_DEBUG
    Iterator(T* data)
        : data(data) {}
#endif
public:
    Iterator() = default;

    Iterator(T* data, const T* begin, const T* end)
        : data(data)
#ifdef SPH_DEBUG
        , begin(begin)
        , end(end)
#endif
    {
        ASSERT(end >= begin, begin, end);
    }

    INLINE const TValue& operator*() const {
        ASSERT_UNEVAL(data >= begin && data < end);
        return *data;
    }
    INLINE TValue& operator*() {
        ASSERT_UNEVAL(data >= begin && data < end);
        return *data;
    }
    INLINE T* operator->() {
        ASSERT_UNEVAL(data >= begin && data < end);
        return data;
    }
    INLINE const T* operator->() const {
        ASSERT_UNEVAL(data >= begin && data < end);
        return data;
    }

#ifdef SPH_DEBUG
    INLINE Iterator operator+(const TCounter n) const {
        return Iterator(data + n, begin, end);
    }
    INLINE Iterator operator-(const TCounter n) const {
        return Iterator(data - n, begin, end);
    }
#else
    INLINE Iterator operator+(const TCounter n) const {
        return Iterator(data + n);
    }
    INLINE Iterator operator-(const TCounter n) const {
        return Iterator(data - n);
    }
#endif
    INLINE void operator+=(const TCounter n) {
        data += n;
    }
    INLINE void operator-=(const TCounter n) {
        data -= n;
    }
    INLINE Iterator& operator++() {
        ++data;
        return *this;
    }
    INLINE Iterator operator++(int) {
        Iterator tmp(*this);
        operator++();
        return tmp;
    }
    INLINE Iterator& operator--() {
        --data;
        return *this;
    }
    INLINE Iterator operator--(int) {
        Iterator tmp(*this);
        operator--();
        return tmp;
    }
    INLINE Size operator-(const Iterator& iter) const {
        ASSERT(data >= iter.data);
        return data - iter.data;
    }
    INLINE bool operator<(const Iterator& iter) const {
        return data < iter.data;
    }
    INLINE bool operator>(const Iterator& iter) const {
        return data > iter.data;
    }
    INLINE bool operator<=(const Iterator& iter) const {
        return data <= iter.data;
    }
    INLINE bool operator>=(const Iterator& iter) const {
        return data >= iter.data;
    }
    INLINE bool operator==(const Iterator& iter) const {
        return data == iter.data;
    }
    INLINE bool operator!=(const Iterator& iter) const {
        return data != iter.data;
    }
    INLINE operator Iterator<const T>() const {
#ifdef SPH_DEBUG
        return Iterator<const T>(data, begin, end);
#else
        return Iterator<const T>(data, nullptr, nullptr);
#endif
    }

    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = Size;
    using pointer = T*;
    using reference = T&;
};

NAMESPACE_SPH_END
