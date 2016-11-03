#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <type_traits>

NAMESPACE_SPH_BEGIN

struct NothingType {};

const NothingType NOTHING;


/// Wrapper of type value of which may or may not be present. Similar to std::optional comming in c++17.
/// http://en.cppreference.com/w/cpp/utility/optional
template <typename Type>
class Optional : public Object {
private:
    using RawType      = std::remove_reference_t<Type>;
    using ConstRawType = const RawType;

    using StorageType =
        std::conditional_t<std::is_lvalue_reference<Type>::value, std::reference_wrapper<RawType>, RawType>;


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
    Optional(const RawType& t) {
        new (storage) StorageType(t);
        used = true;
    }

    /// Move constuctor from stored value. Initialize the optional value.
    Optional(RawType&& t) {
        new (storage) StorageType(std::move(t));
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
            new (storage) StorageType(std::move(other.get()));
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
    Optional& operator=(const RawType& t) {
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
    Optional& operator=(RawType&& t) {
        if (!used) {
            new (storage) StorageType(std::move(t));
            used = true;
        } else {
            destroy();
            get() = std::move(t);
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

    RawType& get() {
        ASSERT(used);
        return (RawType&)*reinterpret_cast<StorageType*>(storage);
    }

    ConstRawType& get() const {
        ASSERT(used);
        return (ConstRawType&)*reinterpret_cast<const StorageType*>(storage);
    }

    ConstRawType* operator->() const {
        ASSERT(used);
        return &get();
    }

    RawType* operator->() {
        ASSERT(used);
        return &get();
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
