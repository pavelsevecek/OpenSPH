#pragma once

/// \file ArrayView.h
/// \brief Simple non-owning view of a container
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/utility/Iterator.h"

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

    INLINE ArrayView()
        : data(nullptr) {}

    INLINE ArrayView(StorageType* data, TCounter size)
        : data(data)
        , actSize(size) {}

    INLINE ArrayView(const ArrayView& other)
        : data(other.data)
        , actSize(other.actSize) {}

    /// Explicitly initialize to nullptr
    INLINE ArrayView(std::nullptr_t)
        : data(nullptr) {}

    /// Copy operator
    INLINE ArrayView& operator=(const ArrayView& other) {
        this->data = other.data;
        this->actSize = other.actSize;
        return *this;
    }

    /// Implicit conversion to const version
    INLINE operator ArrayView<const T, TCounter>() const {
        return ArrayView<const T, TCounter>(data, actSize);
    }

    INLINE Iterator<StorageType> begin() {
        return Iterator<StorageType>(data, data, data + actSize);
    }

    INLINE Iterator<const StorageType> begin() const {
        return Iterator<const StorageType>(data, data, data + actSize);
    }

    INLINE Iterator<StorageType> end() {
        return Iterator<StorageType>(data + actSize, data, data + actSize);
    }

    INLINE Iterator<const StorageType> end() const {
        return Iterator<const StorageType>(data + actSize, data, data + actSize);
    }

    INLINE T& operator[](const TCounter idx) {
        SPH_ASSERT(unsigned(idx) < unsigned(actSize), idx, actSize);
        return data[idx];
    }

    INLINE const T& operator[](const TCounter idx) const {
        SPH_ASSERT(unsigned(idx) < unsigned(actSize), idx, actSize);
        return data[idx];
    }

    INLINE T& front() {
        SPH_ASSERT(actSize > 0);
        return data[0];
    }

    INLINE const T& front() const {
        SPH_ASSERT(actSize > 0);
        return data[0];
    }

    INLINE T& back() {
        SPH_ASSERT(actSize > 0);
        return data[actSize - 1];
    }

    INLINE const T& back() const {
        SPH_ASSERT(actSize > 0);
        return data[actSize - 1];
    }

    INLINE TCounter size() const {
        return this->actSize;
    }

    INLINE bool empty() const {
        return this->actSize == 0;
    }

    /// \brief Returns a subset of the arrayview.
    INLINE ArrayView subset(const TCounter start, const TCounter length) const {
        SPH_ASSERT(start + length <= size());
        return ArrayView(data + start, length);
    }

    INLINE bool operator!() const {
        return data == nullptr;
    }

    INLINE explicit operator bool() const {
        return data != nullptr;
    }

    /// \brief Comparison operator, comparing arrayviews element-by-element.
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

    /// \brief Inequality operator
    bool operator!=(const ArrayView& other) const {
        return !(*this == other);
    }

    /// \brief Prints content of arrayview to stream.
    ///
    /// Stored values must have overloaded << operator.
    friend std::ostream& operator<<(std::ostream& stream, const ArrayView& array) {
        for (const T& t : array) {
            stream << t << std::endl;
        }
        return stream;
    }
};

template <typename T>
INLINE ArrayView<T> getSingleValueView(T& value) {
    return ArrayView<T>(&value, 1);
}

NAMESPACE_SPH_END
