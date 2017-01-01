#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"
#include "objects/Object.h"
#include <functional>
#include <type_traits>

NAMESPACE_SPH_BEGIN

struct NothingType {};

const NothingType NOTHING;


/// Wrapper of type value of which may or may not be present. Similar to std::optional comming in c++17.
/// http://en.cppreference.com/w/cpp/utility/optional
template <typename Type>
class Optional  {
private:
    using RawType = std::remove_reference_t<Type>;
    using StorageType = typename WrapReferenceType<Type>::Type;
    static constexpr bool isRvalueRef = std::is_rvalue_reference<Type>::value;
    using ReturnType = std::conditional_t<isRvalueRef, Type&&, Type&>;

    alignas(StorageType) char storage[sizeof(StorageType)];
    bool used = false;

    void destroy() {
        if (used) {
            (reinterpret_cast<StorageType*>(storage))->~StorageType();
        }
    }

public:
    Optional() = default;

    /// Copy constuctor from stored value. Initialize the optional value.
    template <typename T, typename = std::enable_if_t<std::is_copy_assignable<T>::value>>
    Optional(const T& t) {
        new (storage) StorageType(t);
        used = true;
    }

    /// Move constuctor from stored value. Initialize the optional value.
    Optional(Type&& t) {
        // intentionally using forward instead of move
        new (storage) StorageType(std::forward<Type>(t));
        used = true;
    }

    /// Copy constructor from other optional. Copies the state and if the passed optional is initialized,
    /// copies the value as well.
    Optional(const Optional& other) {
        used = other.used;
        if (used) {
            new (storage) StorageType(other.get());
        }
    }

    /// Move constructor from other optional. Copies the state and if the passed optional is initialized,
    /// copies the value as well.
    Optional(Optional&& other) {
        used = other.used;
        if (used) {
            new (storage) StorageType(std::forward<Type>(other.get()));
        }
    }

    /// Construct uninitialized
    Optional(const NothingType&) { used = false; }

    /// Destructor
    ~Optional() { destroy(); }

    /// Constructs the uninitialized object from a list of arguments. If the object was previously
    /// initialized, the stored value is destroyed.
    template <typename... TArgs>
    void emplace(TArgs&&... args) {
        if (used) {
            destroy();
        }
        new (storage) StorageType(std::forward<TArgs>(args)...);
        used = true;
    }


    /// Copy operator
    template <typename T, typename = std::enable_if_t<std::is_assignable<Type, T>::value>>
    Optional& operator=(const T& t) {
        if (!used) {
            new (storage) StorageType(t);
            used = true;
        } else {
            destroy();
            get() = t;
        }
        return *this;
    }

    /// Move operator
    Optional& operator=(Type&& t) {
        if (!used) {
            new (storage) StorageType(std::forward<Type>(t));
            used = true;
        } else {
            destroy();
            get() = std::forward<Type>(t);
        }
        return *this;
    }

    Optional& operator=(const Optional& other) {
        if (!other.used) {
            if (used) {
                used = false;
                destroy();
            }
        } else {
            if (!used) {
                new (storage) StorageType(other.get());
                used = true;
            } else {
                destroy();
                get() = other.get();
            }
        }
        return *this;
    }

    Optional& operator=(Optional&& other) {
        if (!other.used) {
            if (used) {
                used = false;
                destroy();
            }
        } else {
            if (!used) {
                new (storage) StorageType(std::move(other.get()));
                used = true;
            } else {
                destroy();
                get() = std::move(other.get());
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

    ReturnType get() {
        ASSERT(used);
        return (ReturnType) * reinterpret_cast<StorageType*>(storage);
    }

    const ReturnType get() const {
        ASSERT(used);
        return (const ReturnType) * reinterpret_cast<const StorageType*>(storage);
    }

    const RawType* operator->() const {
        ASSERT(used);
        return std::addressof(get());
    }

    RawType* operator->() {
        ASSERT(used);
        return std::addressof(get());
    }

    explicit operator bool() const { return used; }

    bool operator!() const { return !used; }
};

template <typename T1, typename T2>
Optional<T1> optionalCast(const Optional<T2>& opt) {
    if (!opt) {
        return NOTHING;
    }
    return Optional<T1>(T1(opt.get()));
}

template<typename T>
Optional<T&&> optionalForward(T&& value) {
    return Optional<T&&>(std::forward<T>(value));
}

template <typename T>
bool operator==(const Optional<T>& opt, const T& t) {
    if (!opt) {
        return false;
    }
    return opt.get() == t;
}


template <typename T>
bool operator==(const T& t, const Optional<T>& opt) {
    if (!opt) {
        return false;
    }
    return opt.get() == t;
}

/// Achtung! Even if both optionals are uninitialized, the comparison returns false
template <typename T>
bool operator==(const Optional<T>& opt1, const Optional<T>& opt2) {
    if (!opt1 || !opt2) {
        return false;
    }
    return opt1.get() == opt2.get();
}


template <typename T>
bool operator==(const Optional<T>& opt, const NothingType&) {
    return !opt;
}


NAMESPACE_SPH_END
