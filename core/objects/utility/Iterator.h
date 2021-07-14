#pragma once

/// \file Iterator.h
/// \brief Ordinary iterator for custom containers.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Assert.h"
#include "common/Traits.h"
#include <iterator>

NAMESPACE_SPH_BEGIN

/// \brief Simple (forward) iterator over continuous array of objects of type T.
///
/// Can be used with STL algorithms.
template <typename T>
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
    using TCounter = ptrdiff_t;

    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    Iterator() = default;

    Iterator(T* data, const T* begin, const T* end)
        : data(data)
#ifdef SPH_DEBUG
        , begin(begin)
        , end(end)
#endif
    {
        SPH_ASSERT(end >= begin, begin, end);
    }

    Iterator(std::nullptr_t)
        : data(nullptr)
#ifdef SPH_DEBUG
        , begin(nullptr)
        , end(nullptr)
#endif
    {
    }

    INLINE const TValue& operator*() const {
        SPH_ASSERT(data != nullptr);
        SPH_ASSERT_UNEVAL(data >= begin && data < end);
        return *data;
    }
    INLINE TValue& operator*() {
        SPH_ASSERT(data != nullptr);
        SPH_ASSERT_UNEVAL(data >= begin && data < end);
        return *data;
    }
    INLINE T* operator->() {
        SPH_ASSERT(data != nullptr);
        SPH_ASSERT_UNEVAL(data >= begin && data < end);
        return data;
    }
    INLINE const T* operator->() const {
        SPH_ASSERT(data != nullptr);
        SPH_ASSERT_UNEVAL(data >= begin && data < end);
        return data;
    }

    INLINE explicit operator bool() const {
        return data != nullptr;
    }

#ifdef SPH_DEBUG
    INLINE Iterator operator+(const TCounter n) const {
        SPH_ASSERT(data != nullptr);
        return Iterator(data + n, begin, end);
    }
    INLINE Iterator operator-(const TCounter n) const {
        SPH_ASSERT(data != nullptr);
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
        SPH_ASSERT(data != nullptr);
        data += n;
    }
    INLINE void operator-=(const TCounter n) {
        SPH_ASSERT(data != nullptr);
        data -= n;
    }
    INLINE Iterator& operator++() {
        SPH_ASSERT(data != nullptr);
        ++data;
        return *this;
    }
    INLINE Iterator operator++(int) {
        SPH_ASSERT(data != nullptr);
        Iterator tmp(*this);
        operator++();
        return tmp;
    }
    INLINE Iterator& operator--() {
        SPH_ASSERT(data != nullptr);
        --data;
        return *this;
    }
    INLINE Iterator operator--(int) {
        SPH_ASSERT(data != nullptr);
        Iterator tmp(*this);
        operator--();
        return tmp;
    }
    INLINE difference_type operator-(const Iterator& iter) const {
        // only valid if both iterators are non-nullptr or both are nullptr
        SPH_ASSERT((data != nullptr) == (iter.data != nullptr));
        return data - iter.data;
    }
    INLINE bool operator<(const Iterator& iter) const {
        SPH_ASSERT(data != nullptr && iter.data != nullptr);
        return data < iter.data;
    }
    INLINE bool operator>(const Iterator& iter) const {
        SPH_ASSERT(data != nullptr && iter.data != nullptr);
        return data > iter.data;
    }
    INLINE bool operator<=(const Iterator& iter) const {
        SPH_ASSERT(data != nullptr && iter.data != nullptr);
        return data <= iter.data;
    }
    INLINE bool operator>=(const Iterator& iter) const {
        SPH_ASSERT(data != nullptr && iter.data != nullptr);
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
};

NAMESPACE_SPH_END
