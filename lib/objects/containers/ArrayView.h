#pragma once

/// \file ArrayView.h
/// \brief Simple non-owning view of a container
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"
#include "common/Traits.h"
#include "objects/wrappers/Iterators.h"

NAMESPACE_SPH_BEGIN

/// \brief Object providing safe access to continuous memory of data.
///
/// Useful to write generic code that can be used with any kind of storage where the data are stored
/// consecutively in memory. Commonly used containers are implicitly convertible to ArrayView.
template <typename T, typename TCounter = Size>
class ArrayView {
private:
    using StorageType = typename WrapReferenceType<T>::Type;

    StorageType* data;
    TCounter actSize = 0;

public:
    using Type = T;
    using Counter = TCounter;

    ArrayView()
        : data(nullptr) {}

    ArrayView(StorageType* data, TCounter size)
        : data(data)
        , actSize(size) {}

    ArrayView(const ArrayView& other)
        : data(other.data)
        , actSize(other.actSize) {}

    /// Move constructor
    ArrayView(ArrayView&& other) {
        std::swap(this->data, other.data);
        std::swap(this->actSize, other.actSize);
    }

    /// Explicitly initialize to nullptr
    ArrayView(std::nullptr_t)
        : data(nullptr) {}

    /// Copy operator
    ArrayView& operator=(const ArrayView& other) {
        this->data = other.data;
        this->actSize = other.actSize;
        return *this;
    }

    /// Move operator
    ArrayView& operator=(ArrayView&& other) {
        this->data = other.data;
        this->actSize = other.actSize;
        return *this;
    }

    /// Implicit conversion to const version
    operator ArrayView<const T, TCounter>() {
        return ArrayView<const T, TCounter>(data, actSize);
    }

    Iterator<StorageType, TCounter> begin() {
        return Iterator<StorageType, TCounter>(data, data, data + actSize);
    }

    Iterator<const StorageType, TCounter> begin() const {
        return Iterator<const StorageType, TCounter>(data, data, data + actSize);
    }

    Iterator<StorageType, TCounter> end() {
        return Iterator<StorageType, TCounter>(data + actSize, data, data + actSize);
    }

    Iterator<const StorageType, TCounter> end() const {
        return Iterator<const StorageType, TCounter>(data + actSize, data, data + actSize);
    }

    INLINE T& operator[](const TCounter idx) {
        ASSERT(unsigned(idx) < unsigned(actSize), idx, actSize);
        return data[idx];
    }

    INLINE const T& operator[](const TCounter idx) const {
        ASSERT(unsigned(idx) < unsigned(actSize), idx, actSize);
        return data[idx];
    }

    INLINE TCounter size() const {
        return this->actSize;
    }

    INLINE bool empty() const {
        return this->actSize == 0;
    }

    /// Returns a subset of the arrayview.
    INLINE ArrayView subset(const TCounter start, const TCounter length) {
        ASSERT(start + length <= size());
        return ArrayView(data + start, length);
    }

    INLINE bool operator!() const {
        return data == nullptr;
    }

    INLINE explicit operator bool() const {
        return data != nullptr;
    }

    /// Comparison operator, comparings arrayviews element-by-element.
    bool operator==(const ArrayView& other) const {
        if (actSize != other.actSize) {
            return false;
        }
        for (TCounter i = 0; i < actSize; ++i) {
            if (data[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

    /// Inequality operator
    bool operator!=(const ArrayView& other) const {
        return !(*this == other);
    }

    /// Prints content of arrayview to stream. Stored values must have overloaded << operator.
    friend std::ostream& operator<<(std::ostream& stream, const ArrayView& array) {
        for (const T& t : array) {
            stream << t << std::endl;
        }
        return stream;
    }
};


NAMESPACE_SPH_END
