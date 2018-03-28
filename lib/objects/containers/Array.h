#pragma once

/// \file Array.h
/// \brief Generic dynamically allocated resizable storage
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "math/Math.h"
#include "objects/containers/Allocators.h"
#include "objects/containers/ArrayView.h"
#include <limits>

NAMESPACE_SPH_BEGIN

template <typename T, typename TCounter = Size>
class CopyableArray;

/// Generic dynamically allocated resizable storage. Can also be used with STL algorithms.
template <typename T, typename TCounter = Size>
class Array : public Noncopyable {
    friend class VectorizedArray; // needs to explicitly set actSize

private:
    using StorageType = typename WrapReferenceType<T>::Type;
    StorageType* data = nullptr;
    TCounter actSize = 0;
    TCounter maxSize = 0;

    static constexpr TCounter maxValue = std::numeric_limits<TCounter>::max();

public:
    using Type = T;
    using Counter = TCounter;

    Array() = default;

    /// Constructs array of given size.
    /// \param elementCnt Number of elements to be constructed (using default constructor)
    /// \param allocatedSize Number of allocated elements.
    explicit Array(const TCounter elementCnt, const TCounter allocatedSize = maxValue) {
        this->alloc(elementCnt, allocatedSize);

        // emplace elements
        if (!std::is_trivially_default_constructible<T>::value) {
            for (TCounter i = 0; i < actSize; ++i) {
                new (data + i) StorageType();
            }
        }
    }

    /// \brief Constructs array from initialized list.
    ///
    /// Allocate only enough elements to store the list. Elements are constructed using copy constructor of
    /// stored type.
    Array(std::initializer_list<StorageType> list) {
        actSize = list.size();
        maxSize = actSize;
        data = (StorageType*)malloc(maxSize * sizeof(StorageType));
        TCounter i = 0;
        for (auto& l : list) {
            new (data + i) StorageType(l);
            i++;
        }
    }

    /// Move constructor from array of the same type
    Array(Array&& other) {
        std::swap(data, other.data);
        std::swap(maxSize, other.maxSize);
        std::swap(actSize, other.actSize);
    }

    /// Destructor
    ~Array() {
        // explicitly destroy all constructed elements
        if (!std::is_trivially_destructible<T>::value) {
            for (TCounter i = 0; i < actSize; ++i) {
                data[i].~StorageType();
            }
        }
        if (data) {
            free(data);
            data = nullptr;
        }
    }

    /// Move operator
    Array& operator=(Array&& other) {
        std::swap(data, other.data);
        std::swap(maxSize, other.maxSize);
        std::swap(actSize, other.actSize);
        return *this;
    }

    /// \brief Performs deep-copy of array elements, resizing array if needed.
    ///
    /// This is only way to copy elements, as for Array object deep-copying of elements is forbidden as it is
    /// rarely needed and deleting copy constructor helps us avoid accidental deep-copy, for example when
    /// passing array as an argument of function.
    Array& operator=(const CopyableArray<T, TCounter>& other) {
        const Array& rhs = other;
        this->resize(rhs.size());
        for (TCounter i = 0; i < rhs.size(); ++i) {
            data[i] = rhs[i];
        }
        return *this;
    }

    /// For l-value references assign each value (does not actually move anything). Works only for arrays of
    /// same size, for simplicity.
    template <typename U, typename = std::enable_if_t<std::is_lvalue_reference<T>::value, U>>
    Array& operator=(Array<U>&& other) {
        ASSERT(this->size() == other.size());
        for (TCounter i = 0; i < other.size(); ++i) {
            (*this)[i] = std::forward<U>(other[i]);
        }
        return *this;
    }

    /// \brief Performs a deep copy of all elements of the array.
    ///
    /// All elements are created using copy-constructor.
    Array clone() const {
        Array newArray;
        newArray.reserve(actSize);
        for (const T& value : *this) {
            newArray.emplaceBack(value);
        }
        return newArray;
    }

    INLINE T& operator[](const TCounter idx) noexcept {
        ASSERT(unsigned(idx) < unsigned(actSize), idx, actSize);
        return data[idx];
    }

    INLINE const T& operator[](const TCounter idx) const noexcept {
        ASSERT(unsigned(idx) < unsigned(actSize), idx, actSize);
        return data[idx];
    }

    INLINE T& front() noexcept {
        ASSERT(actSize > 0);
        return data[0];
    }

    INLINE const T& front() const noexcept {
        ASSERT(actSize > 0);
        return data[0];
    }

    INLINE T& back() noexcept {
        ASSERT(actSize > 0);
        return data[actSize - 1];
    }

    INLINE const T& back() const noexcept {
        ASSERT(actSize > 0);
        return data[actSize - 1];
    }

    /// \brief Sets all elements of the array to given value.
    void fill(const T& t) {
        for (auto& v : *this) {
            v = t;
        }
    }

    INLINE TCounter size() const noexcept {
        return actSize;
    }

    INLINE bool empty() const noexcept {
        return actSize == 0;
    }

    /// \brief Resizes the array to new size.
    ///
    /// This potentially allocated more memory than required, to speed up the allocations. If the new size is
    /// bigger than the current size, new elements are created using default constructor, all currently stored
    /// values (within interval [0, newSize-1]) are preserved, possibly moved using their move constructor.
    /// If the new size is lower than the current size, elements at the end of the array are destroyed.
    /// However, the array is not reallocated, the size is kept for future growth.
    ///
    /// \attention This invalidates all references, pointers, iterators, array views, etc. pointed to the
    /// elements of the array.
    void resize(const TCounter newSize) {
        // check suspiciously high values
        ASSERT(newSize < (std::numeric_limits<TCounter>::max() >> 1));
        if (newSize <= maxSize) {
            // enough elements is already allocated
            if (newSize >= actSize) {
                // enlarging array, we need to construct some new elements
                if (!std::is_trivially_default_constructible<T>::value) {
                    for (TCounter i = actSize; i < newSize; ++i) {
                        new (data + i) StorageType();
                    }
                }
            } else {
                if (!std::is_trivially_destructible<T>::value) {
                    // shrinking array, we need to delete some elements
                    for (TCounter i = newSize; i < actSize; ++i) {
                        data[i].~StorageType();
                    }
                }
            }
        } else {
            // requested more elements than allocate, need to reallocated.
            // allocate twice the current number or the new size, whatever is higher, to avoid frequent
            // reallocation when pushing elements one by one
            const TCounter actNewSize = max(2 * maxSize, newSize);
            Array newArray(0, actNewSize);
            // copy all elements into the new array, using move constructor
            for (TCounter i = 0; i < actSize; ++i) {
                new (newArray.data + i) StorageType(std::move(data[i]));
            }
            // default-construct new elements
            if (!std::is_trivially_default_constructible<T>::value) {
                for (TCounter i = actSize; i < newSize; ++i) {
                    new (newArray.data + i) StorageType();
                }
            }
            // move the array into this
            *this = std::move(newArray);
        }
        actSize = newSize;
    }

    /// \brief Resizes the array to new size and assigns a given value to all newly created elements.
    ///
    /// Previously stored elements are not modified, they are possibly moved using their move operator.
    /// If the new size is lower than the current size, elements at the end are destroyed; the given value is
    /// then irrelevant.
    ///
    /// \attention This invalidates all references, pointers, iterators, array views, etc. pointed to the
    /// elements of the array.
    void resizeAndSet(const TCounter newSize, const T& value) {
        const TCounter currentSize = actSize;
        this->resize(newSize);
        for (TCounter i = currentSize; i < actSize; ++i) {
            (*this)[i] = value;
        }
    }

    /// \brief Allocates enough memory to store the given number of elements.
    ///
    ///  If enough memory is already allocated, the function does nothing.
    ///
    /// \attention This invalidates all references, pointers, iterators, array views, etc. pointed to the
    /// elements of the array.
    void reserve(const TCounter newMaxSize) {
        ASSERT(newMaxSize < (std::numeric_limits<TCounter>::max() >> 1));
        if (newMaxSize > maxSize) {
            const TCounter actNewSize = max(2 * maxSize, newMaxSize);
            Array newArray;
            // don't use the Array(0, actNewSize) constructor to allow using emplaceBack for types without
            // default constructor
            newArray.alloc(0, actNewSize);
            // copy all elements into the new array, using move constructor
            for (TCounter i = 0; i < actSize; ++i) {
                new (newArray.data + i) StorageType(std::move(data[i]));
            }
            newArray.actSize = actSize;
            // move the array into this
            *this = std::move(newArray);
        }
    }

    /// Adds new element to the end of the array, resizing the array if necessary.
    template <typename U>
    INLINE void push(U&& u) {
        resize(actSize + 1);
        data[actSize - 1] = std::forward<U>(u);
    }

    template <typename U>
    void pushAll(const Iterator<const U> first, const Iterator<const U> last) {
        for (Iterator<const U> iter = first; iter != last; ++iter) {
            push(*iter);
        }
    }

    void pushAll(const Array& other) {
        reserve(actSize + other.size());
        pushAll(other.cbegin(), other.cend());
    }

    void pushAll(Array&& other) {
        reserve(actSize + other.size());
        for (T& value : other) {
            push(std::move(value));
        }
    }

    /// \brief Constructs a new element at the end of the array in place, using the provided arguments.
    template <typename... TArgs>
    void emplaceBack(TArgs&&... args) {
        reserve(actSize + 1);
        ASSERT(maxSize > actSize);
        new (data + actSize) StorageType(std::forward<TArgs>(args)...);
        actSize++;
    }

    /// \brief Inserts a new element to given position in the array.
    ///
    /// All the existing elements after the given positions are moved using move operator.
    template <typename U>
    void insert(const TCounter position, U&& value) {
        ASSERT(position <= actSize);
        this->resize(actSize + 1);
        std::move_backward(this->begin() + position, this->end() - 1, this->end());
        data[position] = std::forward<U>(value);
    }

    /// \brief Removes the last element from the array and return its value.
    ///
    /// Asserts if the array is empty.
    INLINE T pop() {
        ASSERT(actSize > 0);
        T value = data[actSize - 1];
        resize(actSize - 1);
        return value;
    }

    /// Removes an element with given index from the array.
    void remove(const TCounter idx) {
        ASSERT(idx < actSize);
        for (TCounter i = idx; i < actSize - 1; ++i) {
            data[i] = std::move(data[i + 1]);
        }
        resize(actSize - 1);
    }

    /// Removes all elements from the array, but does NOT release the memory.
    void clear() {
        if (!std::is_trivially_destructible<T>::value) {
            for (TCounter i = 0; i < actSize; ++i) {
                data[i].~StorageType();
            }
        }
        actSize = 0;
    }

    /// Swaps content of two arrays
    void swap(Array<T, TCounter>& other) {
        std::swap(data, other.data);
        std::swap(maxSize, other.maxSize);
        std::swap(actSize, other.actSize);
    }

    INLINE Iterator<StorageType, TCounter> begin() noexcept {
        return Iterator<StorageType, TCounter>(data, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> begin() const noexcept {
        return Iterator<const StorageType, TCounter>(data, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> cbegin() const noexcept {
        return Iterator<const StorageType, TCounter>(data, data, data + actSize);
    }

    INLINE Iterator<StorageType, TCounter> end() noexcept {
        return Iterator<StorageType, TCounter>(data + actSize, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> end() const noexcept {
        return Iterator<const StorageType, TCounter>(data + actSize, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> cend() const noexcept {
        return Iterator<const StorageType, TCounter>(data + actSize, data, data + actSize);
    }

    /// Implicit conversion to arrayview.
    INLINE operator ArrayView<T, TCounter>() noexcept {
        return ArrayView<T, TCounter>(data, actSize);
    }

    /// Implicit conversion to arrayview, const version.
    INLINE operator ArrayView<const T, TCounter>() const noexcept {
        return ArrayView<const T, TCounter>(data, actSize);
    }

    /// Explicit conversion to arrayview
    ArrayView<T, TCounter> view() noexcept {
        return ArrayView<T, TCounter>(data, actSize);
    }

    /// Explicit conversion to arrayview, const version
    ArrayView<const T, TCounter> view() const noexcept {
        return ArrayView<const T, TCounter>(data, actSize);
    }

    /// Comparison operator, comparings array element-by-element. If arrays differ in number of
    /// constructed elements, the comparison always returns false; allocated size does not play role here.
    bool operator==(const Array& other) const noexcept {
        return view() == other.view();
    }

    /// Inequality operator
    bool operator!=(const Array& other) const noexcept {
        return view() != other.view();
    }

    /// Prints content of array to stream. Stored values must have overloaded << operator.
    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Array& array) {
        for (const T& t : array) {
            stream << t << std::endl;
        }
        return stream;
    }

private:
    void alloc(const TCounter elementCnt, const TCounter allocatedSize) {
        actSize = elementCnt;
        maxSize = allocatedSize;
        if (allocatedSize == maxValue) {
            maxSize = actSize;
        }
        // allocate maxSize elements
        data = (StorageType*)malloc(maxSize * sizeof(StorageType));
    }
};

template <typename T, typename TCounter>
class CopyableArray {
private:
    const Array<T, TCounter>& array;

public:
    CopyableArray(const Array<T, TCounter>& array)
        : array(array) {}

    operator const Array<T, TCounter>&() const {
        return array;
    }
};

/// \todo test
template <typename T, typename TCounter>
INLINE CopyableArray<T, TCounter> copyable(const Array<T, TCounter>& array) {
    return CopyableArray<T, TCounter>(array);
}

/// Creates an array from a list of parameters.
template <typename T0, typename... TArgs>
Array<std::decay_t<T0>> makeArray(T0&& t0, TArgs&&... rest) {
    return Array<std::decay_t<T0>>{ std::forward<T0>(t0), std::forward<TArgs>(rest)... };
}

/// Creates a l-value reference array from a list of l-value references.
template <typename T0, typename... TArgs>
Array<T0&> tieToArray(T0& t0, TArgs&... rest) {
    return Array<T0&>{ t0, rest... };
}


NAMESPACE_SPH_END

/// Overload of std::swap for Sph::Array.
namespace std {
template <typename T, typename TCounter>
void swap(Sph::Array<T, TCounter>& ar1, Sph::Array<T, TCounter>& ar2) {
    ar1.swap(ar2);
}
} // namespace std
