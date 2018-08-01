#pragma once

/// \file AlignedStorage.h
/// \brief Base class for utility wrappers (Optional, Variant, ...)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Traits.h"

NAMESPACE_SPH_BEGIN


/// \brief Simple block of memory on stack with size and alignment given by template type

/// AlignedStorage can be used to construct an object on stack while sidestepping default construction.
/// Objects can be therefore default-constructed even if the underlying type does not have default
/// constructor.
/// Stored object can be later constructed by calling \ref emplace method. Note that when constructed,
/// it has to be later destroyed by explicitly calling \ref destroy method, this is not done
/// automatically! This object does NO checks when the stored value is accessed, or whether it is
/// constructed multiple times. This is left to the user.

// dereferencing type-punned pointer will break strict-aliasing rules
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
/// \todo It's weird that GCC issues this warning as we are using __may_alias__ attribute. Perhaps a bug?

template <typename Type>
class AlignedStorage {
private:
    struct __attribute__((__may_alias__)) Holder {
        alignas(Type) char storage[sizeof(Type)];
    } holder;

public:
    AlignedStorage() = default;

    template <typename... TArgs>
    INLINE void emplace(TArgs&&... rest) {
        new (&holder) Type(std::forward<TArgs>(rest)...);
    }

    INLINE void destroy() {
        get().~Type();
    }

    /// Implicit conversion to stored type
    INLINE constexpr operator Type&() noexcept {
        return get();
    }

    /// Implicit conversion to stored type, const version
    INLINE constexpr operator const Type&() const noexcept {
        return get();
    }

    /// Return the reference to the stored value.
    INLINE constexpr Type& get() noexcept {
        return reinterpret_cast<Type&>(holder);
    }

    /// Returns the reference to the stored value, const version.
    INLINE constexpr const Type& get() const noexcept {
        return reinterpret_cast<const Type&>(holder);
    }
};

/// \brief Specialization for l-value references, a simple wrapper of ReferenceWrapper with same interface to
/// allow generic usage of AlignedStorage for both values and references.
template <typename Type>
class AlignedStorage<Type&> {
    using StorageType = ReferenceWrapper<Type>;

    StorageType storage;

public:
    AlignedStorage() = default;

    template <typename T>
    INLINE void emplace(T& ref) {
        storage = StorageType(ref);
    }

    // no need do explicitly destroy reference wrapper
    INLINE void destroy() {}

    INLINE constexpr operator Type&() noexcept {
        return get();
    }

    /// Implicit conversion to stored type, const version
    INLINE constexpr operator const Type&() const noexcept {
        return get();
    }

    /// Return the reference to the stored value.
    INLINE constexpr Type& get() noexcept {
        return storage;
    }

    /// Returns the reference to the stored value, const version.
    INLINE constexpr const Type& get() const noexcept {
        return storage;
    }
};


NAMESPACE_SPH_END
