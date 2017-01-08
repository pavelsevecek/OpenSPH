#pragma once

/// Generic dynamically allocated resizable storage. Can also be used with STL algorithms
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/ArrayView.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

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

    Array() = default;

    /// Constructs array of given size.
    /// \param elementCnt Number of elements to be constructed (using default constructor)
    /// \param allocatedSize Number of allocated elements.
    explicit Array(const TCounter elementCnt, const TCounter allocatedSize = maxValue)
        : actSize(elementCnt)
        , maxSize(allocatedSize) {
        if (allocatedSize == maxValue) {
            this->maxSize = actSize;
        }
        // allocate maxSize elements
        this->data = (StorageType*)malloc(this->maxSize * sizeof(StorageType));
        // emplace size elements
        if (!std::is_trivially_default_constructible<T>::value) {
            for (TCounter i = 0; i < actSize; ++i) {
                new (data + i) StorageType();
            }
        }
    }

    /// Constructs array from initialized list. Allocate only enough elements to store the list. Elements are
    /// constructed using copy constructor of stored type.
    Array(std::initializer_list<StorageType> list) {
        this->actSize = list.size();
        this->maxSize = this->actSize;
        this->data = (StorageType*)malloc(maxSize * sizeof(StorageType));
        TCounter i = 0;
        for (auto& l : list) {
            new (data + i) StorageType(l);
            i++;
        }
    }

    /// Move constructor from array of the same type
    Array(Array&& other) {
        std::swap(this->data, other.data);
        std::swap(this->maxSize, other.maxSize);
        std::swap(this->actSize, other.actSize);
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
        std::swap(this->data, other.data);
        std::swap(this->maxSize, other.maxSize);
        std::swap(this->actSize, other.actSize);
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

    /// Copy method -- allow to copy array only with an explicit calling of 'clone' to avoid accidental deep
    /// copy.
    Array clone() const {
        Array newArray(this->actSize, maxSize);
        Iterator<StorageType> newIter = newArray.begin();
        Iterator<const StorageType> thisIter = this->begin();
        const Iterator<const StorageType> endIter = thisIter + this->actSize;
        for (; thisIter < endIter; ++thisIter, ++newIter) {
            *newIter = *thisIter;
        }
        return newArray;
    }

    INLINE Iterator<StorageType, TCounter> begin() {
        return Iterator<StorageType>(data, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> begin() const {
        return Iterator<const StorageType>(data, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> cbegin() const {
        return Iterator<const StorageType>(data, data, data + actSize);
    }

    INLINE Iterator<StorageType, TCounter> end() {
        return Iterator<StorageType>(data + actSize, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> end() const {
        return Iterator<const StorageType>(data + actSize, data, data + actSize);
    }

    INLINE Iterator<const StorageType, TCounter> cend() const {
        return Iterator<const StorageType>(data + actSize, data, data + actSize);
    }

    INLINE T& operator[](const TCounter idx) {
        ASSERT(unsigned(idx) < unsigned(actSize));
        return data[idx];
    }

    INLINE const T& operator[](const TCounter idx) const {
        ASSERT(unsigned(idx) < unsigned(actSize));
        return data[idx];
    }

    /// Returns array of references to elements given by a list of indices.
    template <typename... TArgs>
    INLINE auto operator()(const TCounter idx1, const TCounter idx2, const TArgs... rest) {
        return tieToStatic(data[idx1], data[idx2], data[rest]...);
    }

    void fill(const T& t) {
        for (auto& v : *this) {
            v = t;
        }
    }

    INLINE TCounter size() const { return this->actSize; }

    INLINE bool empty() const { return this->actSize == 0; }

    /// Resize the array. All stored values (within interval [0, newSize-1]) are preserved.
    void resize(const TCounter newSize) {
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
                new (newArray.data + i) StorageType(std::move(this->data[i]));
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
        this->actSize = newSize;
    }

    void reserve(const TCounter newMaxSize) {
        if (newMaxSize > maxSize) {
            const TCounter actNewSize = max(2 * maxSize, newMaxSize);
            Array newArray(0, actNewSize);
            // copy all elements into the new array, using move constructor
            for (TCounter i = 0; i < actSize; ++i) {
                new (newArray.data + i) StorageType(std::move(this->data[i]));
            }
            // move the array into this
            *this = std::move(newArray);
        }
    }

    /// Adds new element to the end of the array, resizing the array if necessary.
    template <typename U>
    INLINE void push(U&& u) {
        resize(this->actSize + 1);
        this->data[this->actSize - 1] = std::forward<U>(u);
    }

    template <typename U>
    void pushAll(const Iterator<const U> first, const Iterator<const U> last) {
        for (Iterator<const U> iter = first; iter != last; ++iter) {
            push(*iter);
        }
    }

    void pushAll(const Array& other) {
        //  reserve(actSize + other.size());
        pushAll(other.cbegin(), other.cend());
    }

    /// \todo add tests!
    void pushAll(Array&& other) {
        for (Iterator<T> iter = other.begin(); iter != other.end(); ++iter) {
            push(std::move(*iter));
        }
    }

    /// Removes the last element from the array and return its value. Asserts if the array is empty.
    INLINE T pop() {
        ASSERT(this->actSize > 0);
        T value = data[this->actSize - 1];
        resize(this->actSize - 1);
        return value;
    }

    /// Removes an element with given index from the array.
    void remove(const TCounter idx) {
        ASSERT(idx < this->actSize);
        for (TCounter i = idx; i < this->actSize - 1; ++i) {
            this->data[i] = this->data[i + 1];
        }
        resize(this->actSize - 1);
    }

    /// Removes all elements from the array, but does NOT release the memory.
    void clear() {
        if (!std::is_trivially_destructible<T>::value) {
            for (TCounter i = 0; i < actSize; ++i) {
                data[i].~StorageType();
            }
        }
        this->actSize = 0;
    }

    /// Swaps content of two arrays
    void swap(Array<T, TCounter>& other) {
        std::swap(this->data, other.data);
        std::swap(this->maxSize, other.maxSize);
        std::swap(this->actSize, other.actSize);
    }

    /// Implicit conversion to arrayview.
    INLINE operator ArrayView<T, TCounter>() { return ArrayView<T, TCounter>(data, actSize); }

    /// Implicit conversion to arrayview, const version.
    INLINE operator ArrayView<const T, TCounter>() const {
        return ArrayView<const T, TCounter>(data, actSize);
    }

    /// Explicit conversion to arrayview
    ArrayView<T, TCounter> getView() { return ArrayView<T, TCounter>(data, actSize); }

    /// Explicit conversion to arrayview, const version
    ArrayView<const T, TCounter> getView() const { return ArrayView<const T, TCounter>(data, actSize); }

    /// Comparison operator, comparings array element-by-element. If arrays differ in number of
    /// constructed
    /// elements, the comparison always returns false; allocated size does not play role here.
    bool operator==(const Array& other) const { return getView() == other.getView(); }

    /// Prints content of array to stream. Stored values must have overloaded << operator.
    template<typename TStream>
    friend TStream& operator<<(TStream& stream, const Array& array) {
        for (const T& t : array) {
            stream << t << ", ";
        }
        return stream;
    }
};

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
