#pragma once

/// \file Optional.h
/// \brief Wrapper of type value of which may or may not be present
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)

#include "common/Assert.h"
#include "common/Traits.h"
#include "objects/wrappers/AlignedStorage.h"
#include <type_traits>

NAMESPACE_SPH_BEGIN

struct NothingType {};

const NothingType NOTHING;


/// Wrapper of type value of which may or may not be present. Similar to std::optional comming in c++17.
/// http://en.cppreference.com/w/cpp/utility/optional
template <typename Type>
class Optional {
private:
    using RawType = std::remove_reference_t<Type>;
    using StorageType = typename WrapReferenceType<Type>::Type;

    AlignedStorage<Type> storage;
    bool used = false;

    void destroy() {
        if (used) {
            storage.destroy();
            used = false;
        }
    }

public:
    Optional() = default;

    /// Copy constuctor from stored value. Initialize the optional value.
    template <typename T, typename = std::enable_if_t<std::is_copy_assignable<T>::value>>
    Optional(const T& t) {
        storage.emplace(t);
        used = true;
    }

    /// Move constuctor from stored value. Initialize the optional value.
    Optional(Type&& t) {
        // intentionally using forward instead of move
        storage.emplace(std::forward<Type>(t));
        used = true;
    }

    /// Copy constructor from other optional. Copies the state and if the passed optional is initialized,
    /// copies the value as well.
    Optional(const Optional& other) {
        used = other.used;
        if (used) {
            storage.emplace(other.value());
        }
    }

    /// Move constructor from other optional. Copies the state and if the passed optional is initialized,
    /// copies the value as well. Also un-initializes moved optional.
    Optional(Optional&& other) {
        used = other.used;
        if (used) {
            storage.emplace(std::forward<Type>(other.value()));
        }
        other.used = false;
    }

    /// Construct uninitialized
    Optional(const NothingType&) {
        used = false;
    }

    /// Destructor
    ~Optional() {
        destroy();
    }

    /// Constructs the uninitialized object from a list of arguments. If the object was previously
    /// initialized, the stored value is destroyed.
    template <typename... TArgs>
    void emplace(TArgs&&... args) {
        if (used) {
            destroy();
        }
        storage.emplace(std::forward<TArgs>(args)...);
        used = true;
    }


    /// Copy operator
    template <typename T, typename = std::enable_if_t<std::is_assignable<Type, T>::value>>
    Optional& operator=(const T& t) {
        if (!used) {
            storage.emplace(t);
            used = true;
        } else {
            value() = t;
        }
        return *this;
    }

    /// Move operator
    Optional& operator=(Type&& t) {
        if (!used) {
            storage.emplace(std::forward<Type>(t));
            used = true;
        } else {
            value() = std::forward<Type>(t);
        }
        return *this;
    }

    Optional& operator=(const Optional& other) {
        if (!other) {
            destroy();
        } else {
            if (!used) {
                storage.emplace(other.value());
                used = true;
            } else {
                value() = other.value();
            }
        }
        return *this;
    }

    Optional& operator=(Optional&& other) {
        if (!other.used) {
            destroy();
        } else {
            if (!used) {
                storage.emplace(std::move(other.value()));
                used = true;
            } else {
                value() = std::move(other.value());
            }
        }
        return *this;
    }

    /// Assing 'nothing'.
    Optional& operator=(const NothingType&) {
        if (used) {
            destroy();
        }
        used = false;
        return *this;
    }

    INLINE Type& value() {
        ASSERT(used);
        return storage;
    }

    INLINE const Type& value() const {
        ASSERT(used);
        return storage;
    }

    template <typename TOther>
    INLINE Type valueOr(const TOther& other) const {
        if (used) {
            return storage;
        } else {
            return other;
        }
    }

    INLINE const RawType* operator->() const {
        ASSERT(used);
        return std::addressof(value());
    }

    INLINE RawType* operator->() {
        ASSERT(used);
        return std::addressof(value());
    }

    INLINE explicit operator bool() const {
        return used;
    }

    INLINE bool operator!() const {
        return !used;
    }
};

template <typename T1, typename T2>
Optional<T1> optionalCast(const Optional<T2>& opt) {
    if (!opt) {
        return NOTHING;
    }
    return Optional<T1>(T1(opt.value()));
}

template <typename T>
bool operator==(const Optional<T>& opt, const T& t) {
    if (!opt) {
        return false;
    }
    return opt.value() == t;
}

template <typename T>
bool operator==(const T& t, const Optional<T>& opt) {
    if (!opt) {
        return false;
    }
    return opt.value() == t;
}

/// Achtung! Even if both optionals are uninitialized, the comparison returns false
template <typename T>
bool operator==(const Optional<T>& opt1, const Optional<T>& opt2) {
    if (!opt1 || !opt2) {
        return false;
    }
    return opt1.value() == opt2.value();
}


template <typename T>
bool operator==(const Optional<T>& opt, const NothingType&) {
    return !opt;
}


NAMESPACE_SPH_END
