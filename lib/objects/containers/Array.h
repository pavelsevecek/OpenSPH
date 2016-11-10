#pragma once

/// Generic dynamically allocated resizable storage. Can also be used with STL algorithms
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/ArrayView.h"

NAMESPACE_SPH_BEGIN

template <typename T, typename TCounter = int>
class Array : public Noncopyable {
private:
    using StorageType = typename WrapReferenceType<T>::Type;
    StorageType* data = nullptr;
    TCounter actSize  = 0;
    TCounter maxSize  = 0;

public:
    using Type = T;

    Array() = default;

    /// Constructs array of given size.
    /// \param elementCnt Number of elements to be constructed (using default constructor)
    /// \param allocatedSize Number of allocated elements.
    explicit Array(const TCounter elementCnt, const TCounter allocatedSize = -1)
        : actSize(elementCnt)
        , maxSize(allocatedSize) {
        if (allocatedSize == -1) {
            this->maxSize = actSize;
        }
        // allocate maxSize elements
        this->data = (StorageType*)malloc(this->maxSize * sizeof(StorageType));
        // emplace size elements
        if (!std::is_trivially_constructible<T>::value) {
            for (int i = 0; i < actSize; ++i) {
                new (data + i) StorageType();
            }
        }
    }

    /// Constructs array from initialized list. Allocate only enough elements to store the list. Elements are
    /// constructed using copy constructor of stored type.
    Array(std::initializer_list<StorageType> list) {
        this->actSize = list.size();
        this->maxSize = this->actSize;
        this->data    = (StorageType*)malloc(maxSize * sizeof(StorageType));
        int i         = 0;
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
            for (int i = 0; i < actSize; ++i) {
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
        for (int i = 0; i < other.size(); ++i) {
            (*this)[i] = other[i];
        }
        return *this;
    }

    /// Copy method -- allow to copy array only with an explicit calling of 'clone' to avoid accidental deep
    /// copy.
    Array clone() const {
        Array newArray(this->actSize, maxSize);
        Iterator<StorageType> newIter             = newArray.begin();
        Iterator<const StorageType> thisIter      = this->begin();
        const Iterator<const StorageType> endIter = thisIter + this->actSize;
        for (; thisIter < endIter; ++thisIter, ++newIter) {
            *newIter = *thisIter;
        }
        return newArray;
    }

    Iterator<StorageType, TCounter> begin() { return Iterator<StorageType>(data, data, data + actSize); }

    Iterator<const StorageType, TCounter> begin() const {
        return Iterator<const StorageType>(data, data, data + actSize);
    }

    Iterator<const StorageType, TCounter> cbegin() const {
        return Iterator<const StorageType>(data, data, data + actSize);
    }

    Iterator<StorageType, TCounter> end() {
        return Iterator<StorageType>(data + actSize, data, data + actSize);
    }

    Iterator<const StorageType, TCounter> end() const {
        return Iterator<const StorageType>(data + actSize, data, data + actSize);
    }

    Iterator<const StorageType, TCounter> cend() const {
        return Iterator<const StorageType>(data + actSize, data, data + actSize);
    }

    T& operator[](const TCounter idx) {
        ASSERT(unsigned(idx) < unsigned(actSize));
        return data[idx];
    }

    const T& operator[](const TCounter idx) const {
        ASSERT(unsigned(idx) < unsigned(actSize));
        return data[idx];
    }

    void fill(const T& t) {
        for (auto& v : *this) {
            v = t;
        }
    }

    int size() const { return this->actSize; }

    bool empty() const { return this->actSize == 0; }

    /// Resize the array. All stored values (within interval [0, newSize-1]) are preserved.
    void resize(const int newSize) {
        if (newSize <= maxSize) {
            // enough elements is already allocated
            if (newSize >= actSize) {
                // enlarging array, we need to construct some new elements
                if (!std::is_trivially_constructible<T>::value) {
                    for (int i = actSize; i < newSize; ++i) {
                        new (data + i) StorageType();
                    }
                }
            } else {
                if (!std::is_trivially_destructible<T>::value) {
                    // shrinking array, we need to delete some elements
                    for (int i = newSize; i < actSize; ++i) {
                        data[i].~StorageType();
                    }
                }
            }
        } else {
            // requested more elements than allocate, need to reallocated.
            // allocate twice the current number or the new size, whatever is higher, to avoid frequent
            // reallocation when pushing elements one by one
            const int actNewSize = Math::max(2 * maxSize, newSize);
            Array newArray(0, actNewSize);
            // copy all elements into the new array, using move constructor
            for (int i = 0; i < actSize; ++i) {
                new (newArray.data + i) StorageType(std::move(this->data[i]));
            }
            // default-construct new elements
            if (!std::is_trivially_constructible<T>::value) {
                for (int i = actSize; i < newSize; ++i) {
                    new (newArray.data + i) StorageType();
                }
            }
            // move the array into this
            *this = std::move(newArray);
        }
        this->actSize = newSize;
    }

    void reserve(const int newMaxSize) {
        if (newMaxSize > maxSize) {
            const int actNewSize = Math::max(2 * maxSize, newMaxSize);
            Array newArray(0, actNewSize);
            // copy all elements into the new array, using move constructor
            for (int i = 0; i < actSize; ++i) {
                new (newArray.data + i) StorageType(std::move(this->data[i]));
            }
            // move the array into this
            *this = std::move(newArray);
        }
    }

    /// Adds new element to the end of the array, resizing the array if necessary.
    template <typename U>
    void push(U&& u) {
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

    /// Removes the last element from the array and return its value. Asserts if the array is empty.
    T pop() {
        ASSERT(this->actSize > 0);
        T value = data[this->actSize - 1];
        resize(this->actSize - 1);
        return value;
    }

    /// Removes all elements from the array, but does NOT release the memory.
    void clear() {
        if (!std::is_trivially_destructible<T>::value) {
            for (int i = 0; i < actSize; ++i) {
                data[i].~StorageType();
            }
        }
        this->actSize = 0;
    }

    /// Implicit conversion to arrayview.
    operator ArrayView<T, TCounter>() { return ArrayView<T, TCounter>(data, actSize); }

    /// Implicit conversion to arrayview, const version.
    operator ArrayView<const T, TCounter>() const { return ArrayView<const T, TCounter>(data, actSize); }

    /// Explicit conversion to arrayview
    ArrayView<T, TCounter> getView() { return ArrayView<T, TCounter>(data, actSize); }

    /// Explicit conversion to arrayview, const version
    ArrayView<const T, TCounter> getView() const { return ArrayView<const T, TCounter>(data, actSize); }

    /// Comparison operator, comparings array element-by-element. If arrays differ in number of constructed
    /// elements, the comparison always returns false; allocated size does not play role here.
    bool operator==(const Array<T, TCounter>& other) const {
        if (actSize != other.actSize) {
            return false;
        }
        for (int i = 0; i < actSize; ++i) {
            if (data[i] != other[i]) {
                return false;
            }
        }
        return true;
    }
};

/// Creates an array from a list of parameters.
template <typename T0, typename... TArgs>
Array<std::decay_t<T0>> makeArray(T0&& t0, TArgs&&... rest) {
    return Array<std::decay_t<T0>>{ std::forward<T0>(t0), std::forward<TArgs>(rest)... };
}

/// Creates a l-value reference array from a list of l-value references.
template <typename T0, typename... TArgs>
Array<T0&> refs(T0& t0, TArgs&... rest) {
    return Array<T0&>{ t0, rest... };
}


template <typename T, int N>
class StaticArray : public Noncopyable {
private:
    T data[N];

public:
    StaticArray() = default;

    StaticArray(StaticArray&& other) { std::swap(data, other.data); }

    StaticArray(std::initializer_list<T> list) {
        int n = 0;
        for (auto& i : list) {
            data[n++] = i;
        }
    }

    StaticArray& operator=(StaticArray&& other) {
        std::swap(data, other.data);
        return *this;
    }

    StaticArray clone() const {
        StaticArray cloned;
        for (int i = 0; i < N; ++i) {
            cloned[i] = data[i];
        }
        return cloned;
    }

    T& operator[](const int idx) {
        ASSERT(idx >= 0 && idx < N);
        return data[idx];
    }

    const T& operator[](const int idx) const {
        ASSERT(idx >= 0 && idx < N);
        return data[idx];
    }

    constexpr int size() const { return N; }

    Iterator<T> begin() { return Iterator<T>(data, data, data + N); }

    Iterator<const T> begin() const { return Iterator<T>(data, data, data + N); }

    Iterator<T> end() { return Iterator<T>(data + N, data, data + N); }

    Iterator<const T> end() const { return Iterator<T>(data + N, data, data + N); }

    operator ArrayView<T>() { return ArrayView<T>(data, N); }

    operator ArrayView<const T>() const { return ArrayView<const T>(data, N); }
};

NAMESPACE_SPH_END
