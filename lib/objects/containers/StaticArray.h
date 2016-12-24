#pragma once

#include "objects/containers/ArrayView.h"
#include "objects/wrappers/AlignedStorage.h"

NAMESPACE_SPH_BEGIN

const struct EmptyArray {
} EMPTY_ARRAY;

template <typename T, int N>
class StaticArray : public Object {
private:
    AlignedStorage<T> data[N];
    int actSize;

    using StorageType = WrapReference<T>;

public:
    /// Default constructor, calls default constructor on all elements.
    StaticArray() {
        if (!std::is_trivially_default_constructible<T>::value) {
            for (int i = 0; i < N; ++i) {
                data[i].emplace();
            }
        }
        actSize = N;
    }

    /// Initialize empty array, corresponding to static stack. All N elements are allocated, but none is
    /// constructed and size of the static array is zero.
    StaticArray(const EmptyArray&) { actSize = 0; }

    /// Initialize from initializer list. The size of the list must be smaller or equal to N. All N elements
    /// are all allocated, elements are constructed from initializer list using copy constructor.
    StaticArray(std::initializer_list<StorageType> list) {
        actSize = 0;
        ASSERT(list.size() <= N);
        for (auto& i : list) {
            data[actSize++].emplace(i);
        }
    }

    StaticArray(const StaticArray& other) = delete;

    StaticArray(StaticArray&& other) = default;

    /// Destructor, destroys all elements in the array.
    ~StaticArray() {
        if (!std::is_trivially_destructible<T>::value) {
            for (int i = 0; i < actSize; ++i) {
                data[i].destroy();
            }
        }
        actSize = 0;
    }

    StaticArray& operator=(const StaticArray& other) = delete;

    StaticArray& operator=(StaticArray&& other) = default;

    /// Assignment operator for array of references on left-hand side. Can be used as std::tie.
    template <typename U, typename = std::enable_if_t<std::is_lvalue_reference<T>::value, U>>
    StaticArray& operator=(StaticArray<U, N>&& other) {
        ASSERT(this->size() == other.size());
        for (int i = 0; i < other.size(); ++i) {
            (*this)[i] = std::move(other[i]);
        }
        return *this;
    }

    /// Clones the array, calling copy constructor on all elements. The size of the cloned array corresponds
    /// to current size of this array.
    StaticArray clone() const {
        StaticArray cloned(EMPTY_ARRAY);
        for (int i = 0; i < actSize; ++i) {
            cloned.push(data[i].get());
        }
        return cloned;
    }

    INLINE T& operator[](const int idx) noexcept {
        ASSERT(idx >= 0 && idx < actSize);
        return data[idx].get();
    }

    INLINE const T& operator[](const int idx) const noexcept {
        ASSERT(idx >= 0 && idx < actSize);
        return data[idx].get();
    }

    /// Maximum allowed size of the array.
    INLINE constexpr int maxSize() const noexcept { return N; }

    /// Current size of the array (number of constructed elements). Can be between 0 and N.
    INLINE int size() const { return actSize; }

    /// Inserts a value to the end of the array using copy/move constructor. The current size of the array
    /// must be less than N, checked by assert.
    template <typename U, typename = std::enable_if_t<std::is_constructible<StorageType, U>::value>>
    INLINE void push(U&& value) {
        ASSERT(actSize < N);
        data[actSize++].emplace(std::forward<U>(value));
    }

    /// Removes and destroys element from the end of the array. The removed element is returned from the
    /// function. Array must not be empty, checked by assert.
    INLINE T pop() {
        ASSERT(actSize > 0);
        T value = data[actSize - 1];
        data[actSize - 1].destroy();
        actSize--;
        return value;
    }

    /// Changes size of the array. New size must be between 0 and N. If the array is shrinked, elements from
    /// the end of the array are destroyed; if the array is enlarged, new elements are created using default
    /// constructor.
    void resize(const int newSize) {
        ASSERT(unsigned(newSize) <= N);
        if (!std::is_trivially_default_constructible<T>::value) {
            if (newSize > actSize) {
                for (int i = actSize; i < newSize; ++i) {
                    data[i].emplace();
                }
            } else {
                for (int i = newSize; i < actSize; ++i) {
                    data[i].destroy();
                }
            }
        }
        actSize = newSize;
    }

    Iterator<T> begin() { return Iterator<T>(rawData(), rawData(), rawData() + actSize); }

    Iterator<const T> begin() const { return Iterator<T>(rawData(), rawData(), rawData() + actSize); }

    Iterator<T> end() { return Iterator<T>(rawData() + actSize, rawData(), rawData() + actSize); }

    Iterator<const T> end() const { return Iterator<T>(rawData() + actSize, rawData(), rawData() + actSize); }

    operator ArrayView<T>() { return ArrayView<T>(rawData(), actSize); }

    operator ArrayView<const T>() const { return ArrayView<const T>(rawData(), actSize); }

private:
    INLINE StorageType* rawData() { return reinterpret_cast<StorageType*>(data); }
};

/// Creates a static array from a list of parameters. All parameters must have the same type. Both the
/// allocated size of the array and the number of constructed elements equal to the number of parameters.
template <typename T0, typename... TArgs>
StaticArray<T0, sizeof...(TArgs) + 1> makeStatic(T0&& t0, TArgs&&... rest) {
    return StaticArray<T0, sizeof...(TArgs) + 1>({std::forward<T0>(t0), std::forward<TArgs>(rest)...});
}

/// Creates a static array from a list of l-value references. All parameters must have the same type. Both the
/// allocated size of the array and the number of constructed elements equal to the number of parameters.
template <typename T0, typename... TArgs>
StaticArray<T0&, sizeof...(TArgs) + 1> tieToStatic(T0& t0, TArgs&... rest) {
    return StaticArray<T0&, sizeof...(TArgs) + 1>({t0, rest...});
}

NAMESPACE_SPH_END
