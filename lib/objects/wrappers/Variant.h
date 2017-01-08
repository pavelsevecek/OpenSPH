#pragma once

/// Object capable of storing values of different time, having only one value (and one type) in any moment.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"
#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// Stores value of type std::aligned_union, having size and alignment equal to maximum of sizes and
/// alignments of template types.
template <typename... TArgs>
class AlignedUnion {
private:
    std::aligned_union_t<4, TArgs...> storage;

public:
    /// Creates value of type T in place. Does not check whether another object has already been created.
    template <typename T, typename... Ts>
    INLINE void emplace(Ts&&... args) {
        new (&storage) T(std::forward<Ts>(args)...);
    }

    /// Destroys value of type T. Does not check that the value of type T is currently stored
    template <typename T>
    INLINE void destroy() {
        get<T>().~T();
    }

    /// Converts stored value to a reference of type T, without checking that the currently stored value has
    /// such a type.
    template <typename T>
    INLINE T& get() {
        return reinterpret_cast<T&>(storage);
    }

    /// Converts stored value to a reference of type T, without checking that the currently stored value has
    /// such a type, const version.
    template <typename T>
    INLINE const T& get() const {
        return reinterpret_cast<const T&>(storage);
    }
};

/// Helper visitors creating, deleting or modifying Variant
namespace VariantHelpers {

    /// Creates a variant using value type of which can be stored in variant.
    template <typename... TArgs>
    struct Create {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            using TRaw = std::decay_t<TOther>;
            storage.template emplace<TRaw>(std::forward<TOther>(other));
        }
    };

    /// Destroys the object currently stored in variant.
    template <typename... TArgs>
    struct Delete {
        AlignedUnion<TArgs...>& storage;

        template <typename T>
        void visit() {
            storage.template destroy<T>();
        }
    };

    /// Creates a variant by copying/moving value currently stored in other variant.
    template <typename... TArgs>
    struct CopyMoveCreate {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            if (std::is_lvalue_reference<TOther>::value) {
                storage.template emplace<T>(other.template get<T>());
            } else {
                storage.template emplace<T>(std::move(other.template get<T>()));
            }
        }
    };

    /// Assigns a value type of which can be stored in variant.
    template <typename... TArgs>
    struct Assign {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            using TRaw = std::decay_t<TOther>;
            storage.template get<TRaw>() = std::forward<TOther>(other);
        }
    };

    /// Creates a variant by copying/moving value currently stored in other variant.
    template <typename... TArgs>
    struct CopyMoveAssign {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            if (std::is_lvalue_reference<TOther>::value) {
                storage.template get<T>() = other.template get<T>();
            } else {
                storage.template get<T>() = std::move(other.template get<T>());
            }
        }
    };
}

/// Iterates through types of variant until idx-th type is found, and executes given TVisitor, passing
/// arguments to its method visit().
template <typename T0, typename... TArgs>
struct VariantIterator {
    template <typename TVisitor, typename... Ts>
    static void visit(int idx, TVisitor&& visitor, Ts&&... args) {
        if (idx == 0) {
            visitor.template visit<T0>(std::forward<Ts>(args)...);
        } else {
            VariantIterator<TArgs...>::visit(
                idx - 1, std::forward<TVisitor>(visitor), std::forward<Ts>(args)...);
        }
    }
};
/// Specialization for last type
template <typename T0>
struct VariantIterator<T0> {
    template <typename TVisitor, typename... Ts>
    static void visit(int UNUSED_IN_RELEASE(idx), TVisitor&& visitor, Ts&&... args) {
        ASSERT(idx == 0);
        visitor.template visit<T0>(std::forward<Ts>(args)...);
    }
};


/// Variant, an implementation of type-safe union.
template <typename... TArgs>
class Variant {
private:
    AlignedUnion<TArgs...> storage;
    int typeIdx = -1;

    void destroy() {
        if (typeIdx != -1) {
            VariantHelpers::Delete<TArgs...> deleter{ storage };
            VariantIterator<TArgs...>::visit(typeIdx, deleter);
        }
    }

public:
    Variant() = default;

    ~Variant() { destroy(); }

    /// Construct variant from value of stored type
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    Variant(T&& value) {
        using RawT = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        storage.template emplace<RawT>(std::forward<T>(value));
        typeIdx = idx;
    }

    Variant(const Variant& other) {
        VariantHelpers::CopyMoveCreate<TArgs...> creator{ storage };
        VariantIterator<TArgs...>::visit(other.typeIdx, creator, other);
        typeIdx = other.typeIdx;
    }

    Variant(Variant&& other) {
        VariantHelpers::CopyMoveCreate<TArgs...> creator{ storage };
        VariantIterator<TArgs...>::visit(other.typeIdx, creator, std::move(other));
        std::swap(typeIdx, other.typeIdx);
    }

    /// Universal copy/move operator with type of rhs being one of stored types. Valid type is checked by
    /// static assert. If the type of rhs is the same as the type currently stored in variant, the value is
    /// copy/move assigned, otherwise the current value is destroyed and a copy/move constructor is used.
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    Variant& operator=(T&& value) {
        using RawT = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        if (typeIdx != idx) {
            // different type, destroy current and re-create
            destroy();
            storage.template emplace<RawT>(std::forward<T>(value));
        } else {
            // same type, can utilize assignment operator
            storage.template get<RawT>() = std::forward<T>(value);
        }
        typeIdx = idx;
        return *this;
    }

    /// Assigns another variant into this. If the type of the currently stored value is the same as type in
    /// rhs, the copy operator is utilized, otherwise the current value (if there is one) is destroyed and a
    /// new value is created using copy constructor.
    Variant& operator=(const Variant& other) {
        if (typeIdx != other.typeIdx) {
            destroy();
            VariantHelpers::CopyMoveCreate<TArgs...> creator{ storage };
            VariantIterator<TArgs...>::visit(other.typeIdx, creator, other);
        } else {
            VariantHelpers::CopyMoveAssign<TArgs...> assigner{ storage };
            VariantIterator<TArgs...>::visit(other.typeIdx, assigner, other);
        }
        typeIdx = other.typeIdx;
        return *this;
    }

    /// Moves another variant into this. If the type of the currently stored value is the same as type in
    /// rhs, the move operator is utilized, otherwise the current value (if there is one) is destroyed and a
    /// new value is created using move constructor. Type index of the rhs variant is set to 'uninitialized'
    /// (-1) to indicate the value has been moved and is no longer usable.
    Variant& operator=(Variant&& other) {
        if (typeIdx != other.typeIdx) {
            destroy();
            VariantHelpers::CopyMoveCreate<TArgs...> creator{ storage };
            VariantIterator<TArgs...>::visit(other.typeIdx, creator, std::move(other));
        } else {
            VariantHelpers::CopyMoveAssign<TArgs...> assigner{ storage };
            VariantIterator<TArgs...>::visit(other.typeIdx, assigner, std::move(other));
        }
        typeIdx = other.typeIdx;
        other.typeIdx = -1;
        return *this;
    }

    /// Creates a value of type T in place. Type T must be one of variant types. Any previously stored value
    /// is destroyed.
    template <typename T, typename... Ts>
    void emplace(Ts&&... args) {
        using RawT = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        destroy();
        storage.template emplace<RawT>(std::forward<Ts>(args)...);
        typeIdx = idx;
    }

    /// Returns index of type currently stored in variant. If no value is currently stored, returns -1.
    int getTypeIdx() const { return typeIdx; }

    /// Returns the stored value. Performs a compile-time check that the type is contained in Variant, and
    /// runtime check that the variant currently holds value of given type.
    template <typename T>
    INLINE T& get() {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT(typeIdx == idx);
        return storage.template get<T>();
    }

    /// Returns the stored value, const version.
    template <typename T>
    INLINE const T& get() const {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT(typeIdx == idx);
        return storage.template get<T>();
    }

    /// Implicit conversion to one of stored values. Performs a compile-time check that the type is contained
    /// in Variant, and runtime check that the variant currently holds value of given type.
    template <typename T>
    operator T&() {
        return get<T>();
    }

    /// Const version of conversion operator.
    template <typename T>
    operator const T&() const {
        return get<T>();
    }

    /// Returns the stored value in the variant. Safer than implicit conversion as it returns NOTHING in case
    /// the value is currently not stored in variant.
    template <typename T>
    Optional<T> tryGet() const {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        if (typeIdx != getTypeIndex<T, TArgs...>) {
            return NOTHING;
        }
        return storage.template get<T>();
    }
};


namespace Detail {
    template <Size N, typename T0, typename... TArgs>
    struct ForCurrentType {
        template <typename TVariant, typename TFunctor>
        static decltype(auto) action(TVariant&& variant, TFunctor&& functor) {
            if (variant.template getTypeIdx() == N) {
                return functor(variant.template get<T0>());
            } else {
                return ForCurrentType<N + 1, TArgs...>::action(
                    std::forward<TVariant>(variant), std::forward<TFunctor>(functor));
            }
        }
    };
    template <Size N, typename T0>
    struct ForCurrentType<N, T0> {
        template <typename TVariant, typename TFunctor>
        static decltype(auto) action(TVariant&& variant, TFunctor&& functor) {
            return functor(variant.template get<T0>());
        }
    };
}

/// Executes a functor passing current value stored in variant as its parameter. The functor must either have
/// templated operator() (i.e. generic lambda), or have an overloaded operator for each type contained in
/// variant.
/// \param variant Variant, must contain a value (checked by assert).
/// \param functor Executed functor, takes one parameter - value obtained from Variant.
/// \return Value returned by the functor.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) forValue(Variant<TArgs...>& variant, TFunctor&& functor) {
    ASSERT(variant.template getTypeIdx() != -1);
    return Detail::ForCurrentType<0, TArgs...>::action(variant, std::forward<TFunctor>(functor));
}

/// Executes a functor passing current value stored in variant as its parameter. Overload for const l-value
/// reference.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) forValue(const Variant<TArgs...>& variant, TFunctor&& functor) {
    ASSERT(variant.template getTypeIdx() != -1);
    return Detail::ForCurrentType<0, TArgs...>::action(variant, std::forward<TFunctor>(functor));
}


NAMESPACE_SPH_END
