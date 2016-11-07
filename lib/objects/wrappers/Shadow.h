#pragma once

/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN


/// Simple wrapper of type that sidesteps default construction. Shadow objects can be therefore
/// default-constructed even if the underlying type does not have default constructor.
/// Stored object can be later constructed by calling <code>emplace</code> method. Note that when constructed,
/// it has to be later destroyed by explicitly calling <code>destroy</code> method, this is not done
/// automatically! Shadow object does NO checks when the stored value is accessed, or whether it is
/// constructed multiple times. This is left to the user.
template <typename Type>
class Shadow : public Object {
private:
    using StorageT = std::conditional_t<std::is_lvalue_reference<Type>::value,
                                        std::reference_wrapper<std::remove_reference_t<Type>>,
                                        Type>;
    alignas(Type) char storage[sizeof(Type)];

    Type& data() { return *reinterpret_cast<Type*>(storage); }

    const Type& constData() const { return *reinterpret_cast<const Type*>(storage); }

public:
    Shadow() = default;

    template <typename T0, typename... TArgs>
    void emplace(T0&& t0, TArgs&&... rest) {
        new (storage) Type(std::forward<T0>(t0), std::forward<TArgs>(rest)...);
    }

    void destroy() { data().~Type(); }

    /// Implicit conversion to stored type
    operator Type&() { return data(); }

    /// Implicit conversion to stored type, const version
    operator const Type&() { return constData(); }

    /// Return the reference to the stored value.
    Type& get() { return data(); }

    /// Returns the reference to the stored value, const version.
    const Type& get() const { return constData(); }

    Type* operator->() { return &data(); }

    const Type* operator->() const { return &constData(); }
};


NAMESPACE_SPH_END
