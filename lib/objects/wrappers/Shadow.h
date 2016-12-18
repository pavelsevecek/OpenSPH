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
    using StorageType = typename WrapReferenceType<Type>::Type;
    alignas(StorageType) char storage[sizeof(StorageType)];

    INLINE constexpr Type& data() { return *reinterpret_cast<Type*>(storage); }

    INLINE constexpr const Type& constData() const { return *reinterpret_cast<const Type*>(storage); }

public:
    Shadow() = default;

    template <typename... TArgs>
    INLINE void emplace(TArgs&&... rest) {
        new (storage) StorageType(std::forward<TArgs>(rest)...);
    }

    INLINE void destroy() { data().~Type(); }

    /// Implicit conversion to stored type
    INLINE constexpr operator Type&() noexcept { return data(); }

    /// Implicit conversion to stored type, const version
    INLINE constexpr operator const Type&() const noexcept { return constData(); }

    /// Return the reference to the stored value.
    INLINE constexpr Type& get() noexcept { return data(); }

    /// Returns the reference to the stored value, const version.
    INLINE constexpr const Type& get() const noexcept { return constData(); }

    INLINE constexpr Type* operator->() noexcept { return &data(); }

    INLINE constexpr const Type* operator->() const noexcept { return &constData(); }
};


NAMESPACE_SPH_END
