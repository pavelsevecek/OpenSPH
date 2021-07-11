#pragma once

#include "objects/containers/ArrayView.h"
#include "objects/wrappers/AlignedStorage.h"
#include <initializer_list>

NAMESPACE_SPH_BEGIN

const struct EmptyArray {
} EMPTY_ARRAY;

/// \brief Array with fixed number of allocated elements.
///
/// Number of actually created elements can be different and it can also be changed; elements can be created
/// and destroyed, although it is not possible to construct more elements than the maximum allocated number,
/// determined by template parameter N or function \ref maxSize. The array is intentionally non-copyable to
/// avoid accidentaly copying values, where move or pass-by-reference is possible.
template <typename T, int N, typename TCounter = Size>
class StaticArray {
private:
    AlignedStorage<T> data[N];
    TCounter actSize;

    using StorageType = WrapReference<T>;

public:
    /// \brief Default constructor, calls default constructor on all elements.
    StaticArray() {
        if (!std::is_trivially_default_constructible<T>::value) {
            for (TCounter i = 0; i < N; ++i) {
                data[i].emplace();
            }
        }
        actSize = N;
    }

    /// \brief Initialize an empty array.
    ///
    /// This effectively creates an empty stack; all N elements are allocated, but none is constructed and
    /// size of the array is zero.
    StaticArray(const EmptyArray&) {
        actSize = 0;
    }

    /// \brief Initialize using initializer_list.
    ///
    /// The size of the list must be smaller or equal to N. All N elements are all allocated, elements are
    /// constructed from initializer_list using copy constructor.
    StaticArray(std::initializer_list<StorageType> list) {
        actSize = 0;
        SPH_ASSERT(list.size() <= N);
        for (auto& i : list) {
            data[actSize++].emplace(i);
        }
    }

    StaticArray(const StaticArray& other) = delete;

    StaticArray(StaticArray&& other) {
        actSize = 0;
        for (TCounter i = 0; i < other.size(); ++i) {
            // move if it contains values, just copy if it contains references
            data[actSize++].emplace(std::forward<T>(other[i]));
        }
    }

    /// \brief Destructor, destroys all constructed elements in the array.
    ~StaticArray() {
        if (!std::is_trivially_destructible<T>::value) {
            for (TCounter i = 0; i < actSize; ++i) {
                data[i].destroy();
            }
        }
        actSize = 0;
    }

    StaticArray& operator=(const StaticArray& other) = delete;

    /// \brief Move operator for arrays holding default-constructible types.
    ///
    /// Resizes array and moves all elements of the rhs array.
    template <typename U, typename = std::enable_if_t<std::is_default_constructible<T>::value, U>>
    StaticArray& operator=(StaticArray<U, N>&& other) {
        this->resize(other.size());
        for (TCounter i = 0; i < other.size(); ++i) {
            // move if it contains values, just copy if it contains references
            (*this)[i] = std::forward<T>(other[i]);
        }
        return *this;
    }

    /// \brief Special assignment operator for array of references on left-hand side.
    ///
    /// Can be used similarly to std::tie; a function may return multiple values by wrapping them into
    /// StaticArray<T>, these values can be then saved individual variables by wrapping the variables into
    /// \ref StaticArray<T&>. A big advantage over tuple is the option to return a variable number of elements
    /// (i.e. number of arguments is not known at compile time).
    template <typename U, int M, typename = std::enable_if_t<std::is_lvalue_reference<T>::value, U>>
    StaticArray& operator=(StaticArray<U, M>&& other) {
        SPH_ASSERT(this->size() == other.size());
        for (TCounter i = 0; i < other.size(); ++i) {
            (*this)[i] = std::forward<U>(other[i]);
        }
        return *this;
    }

    /// \brief Clones the array, calling copy constructor on all elements.
    ///
    /// The size of the cloned array corresponds to current size of this array.
    StaticArray clone() const {
        StaticArray cloned(EMPTY_ARRAY);
        for (TCounter i = 0; i < actSize; ++i) {
            cloned.push(data[i].get());
        }
        return cloned;
    }

    /// \brief Assigns a value to all constructed elements of the array.
    ///
    /// Does not resize the array.
    void fill(const T& value) {
        for (TCounter i = 0; i < actSize; ++i) {
            data[i].get() = value;
        }
    }

    /// \brief Returns the element with given index.
    INLINE T& operator[](const TCounter idx) noexcept {
        SPH_ASSERT(idx >= 0 && idx < actSize, idx, actSize);
        return data[idx].get();
    }

    /// \brief Returns the element with given index.
    INLINE const T& operator[](const TCounter idx) const noexcept {
        SPH_ASSERT(idx >= 0 && idx < actSize, idx, actSize);
        return data[idx].get();
    }

    /// \brief Returns the maximum allowed size of the array.
    INLINE constexpr TCounter maxSize() const noexcept {
        return N;
    }

    /// \brief Returns the current size of the array (number of constructed elements).
    ///
    /// Can be between 0 and N.
    INLINE TCounter size() const {
        return actSize;
    }

    /// \brief Return true if the array is empty.
    ///
    /// Depends only on number of constructed elements, not allocated size.
    INLINE bool empty() const {
        return actSize == 0;
    }

    /// \brief Inserts a value to the end of the array using copy/move constructor.
    ///
    /// The current size of the array must be less than N, checked by assert.
    template <typename U, typename = std::enable_if_t<std::is_constructible<StorageType, U>::value>>
    INLINE void push(U&& value) {
        SPH_ASSERT(actSize < N);
        data[actSize++].emplace(std::forward<U>(value));
    }

    /// \brief Removes and destroys element from the end of the array.
    ///
    /// The removed element is returned from the function. Array must not be empty, checked by assert.
    INLINE T pop() {
        SPH_ASSERT(actSize > 0);
        T value = data[actSize - 1];
        data[actSize - 1].destroy();
        actSize--;
        return value;
    }

    /// \brief Changes size of the array.
    ///
    /// New size must be between 0 and N. If the array is shrinked, elements from the end of the array are
    /// destroyed; if the array is enlarged, new elements are created using default constructor.
    void resize(const TCounter newSize) {
        SPH_ASSERT(unsigned(newSize) <= N);
        if (!std::is_trivially_default_constructible<T>::value) {
            if (newSize > actSize) {
                for (TCounter i = actSize; i < newSize; ++i) {
                    data[i].emplace();
                }
            } else {
                for (TCounter i = newSize; i < actSize; ++i) {
                    data[i].destroy();
                }
            }
        }
        actSize = newSize;
    }

    INLINE Iterator<StorageType> begin() {
        return Iterator<StorageType>(rawData(), rawData(), rawData() + actSize);
    }

    INLINE Iterator<const StorageType> begin() const {
        return Iterator<const StorageType>(rawData(), rawData(), rawData() + actSize);
    }

    INLINE Iterator<const StorageType> cbegin() const {
        return Iterator<const StorageType>(rawData(), rawData(), rawData() + actSize);
    }

    INLINE Iterator<StorageType> end() {
        return Iterator<StorageType>(rawData() + actSize, rawData(), rawData() + actSize);
    }

    INLINE Iterator<const StorageType> end() const {
        return Iterator<const StorageType>(rawData() + actSize, rawData(), rawData() + actSize);
    }

    INLINE Iterator<const StorageType> cend() const {
        return Iterator<const StorageType>(rawData() + actSize, rawData(), rawData() + actSize);
    }

    operator ArrayView<T>() {
        return ArrayView<T>(rawData(), actSize);
    }

    operator ArrayView<const T>() const {
        return ArrayView<const T>(rawData(), actSize);
    }

    bool operator==(const StaticArray& other) const {
        if (actSize != other.actSize) {
            return false;
        }
        for (TCounter i = 0; i < actSize; ++i) {
            if (data[i].get() != other.data[i].get()) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const StaticArray& other) const {
        return !(*this == other);
    }

    /// \brief Prints content of array to stream.
    ///
    /// Stored values must have overloaded << operator.
    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const StaticArray& array) {
        for (const T& t : array) {
            stream << t << std::endl;
        }
        return stream;
    }

private:
    INLINE StorageType* rawData() {
        return reinterpret_cast<StorageType*>(data);
    }

    INLINE const StorageType* rawData() const {
        return reinterpret_cast<const StorageType*>(data);
    }
};

/// \brief Creates a static array from a list of parameters.
///
/// All parameters must have the same type. Both the allocated size of the array and the number of constructed
/// elements equal to the number of parameters.
template <typename T0, typename... TArgs>
StaticArray<T0, sizeof...(TArgs) + 1> makeStatic(T0&& t0, TArgs&&... rest) {
    return StaticArray<T0, sizeof...(TArgs) + 1>({ std::forward<T0>(t0), std::forward<TArgs>(rest)... });
}

/// \brief Creates a static array from a list of l-value references.
///
/// All parameters must have the same type. Both the allocated size of the array and the number of constructed
/// elements equal to the number of parameters.
template <typename T0, typename... TArgs>
StaticArray<T0&, sizeof...(TArgs) + 1> tie(T0& t0, TArgs&... rest) {
    return StaticArray<T0&, sizeof...(TArgs) + 1>({ t0, rest... });
}

/// \brief Alias for array holding two elements of the same type.
template <typename T>
using Pair = StaticArray<T, 2>;


/// \brief Container similar to \ref StaticArray, but with constexpr constructors and getters.
template <typename T, Size N>
class ConstexprArray {
private:
    T data[N];

public:
    template <typename... TArgs>
    constexpr ConstexprArray(TArgs&&... args)
        : data{ std::forward<TArgs>(args)... } {}

    constexpr const T& operator[](const Size idx) const {
        return data[idx];
    }

    constexpr T& operator[](const Size idx) {
        return data[idx];
    }
};

NAMESPACE_SPH_END
