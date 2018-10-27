#pragma once

/// \file Variant.h
/// \brief Object capable of storing values of different types
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Assert.h"
#include "common/Globals.h"
#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// \brief Stores value of type std::aligned_union, having size and alignment equal to maximum of sizes and
/// alignments of template types.
template <typename... TArgs>
class AlignedUnion {
private:
    std::aligned_union_t<4, TArgs...> storage;

public:
    /// \brief Creates value of type T in place.
    ///
    /// Does not check whether another object has already been created.
    template <typename T, typename... Ts>
    INLINE void emplace(Ts&&... args) {
        new (&storage) T(std::forward<Ts>(args)...);
    }

    /// \brief Destroys value of type T.
    ///
    /// Does not check that the value of type T is currently stored
    template <typename T>
    INLINE void destroy() {
        get<T>().~T();
    }

    /// \brief Converts stored value to a reference of type T.
    ///
    /// Does not check that the currently stored value has such a type.
    template <typename T>
    INLINE T& get() {
        return reinterpret_cast<T&>(storage);
    }

    /// \copydoc get
    template <typename T>
    INLINE const T& get() const {
        return reinterpret_cast<const T&>(storage);
    }
};

/// \brief Helper visitors creating, deleting or modifying Variant
namespace VariantHelpers {

/// \brief Creates a variant using default constructor
template <typename... TArgs>
struct DefaultCreate {
    AlignedUnion<TArgs...>& storage;

    template <typename T>
    void visit() {
        storage.template emplace<T>();
    }
};

/// \brief Destroys the object currently stored in variant.
template <typename... TArgs>
struct Delete {
    AlignedUnion<TArgs...>& storage;

    template <typename T>
    void visit() {
        storage.template destroy<T>();
    }
};

/// \brief Creates a variant by copying/moving value currently stored in other variant.
template <typename... TArgs>
struct CopyMoveCreate {
    AlignedUnion<TArgs...>& storage;

    template <typename T, typename TOther>
    void visit(const TOther& other) {
        storage.template emplace<T>(other.template get<T>());
    }

    template <typename T,
        typename TOther,
        typename = std::enable_if_t<!std::is_lvalue_reference<TOther>::value>>
    void visit(TOther&& other) {
        storage.template emplace<T>(std::move(other.template get<T>()));
    }
};

/// \brief Assigns a value type of which can be stored in variant.
template <typename... TArgs>
struct Assign {
    AlignedUnion<TArgs...>& storage;

    template <typename T, typename TOther>
    void visit(TOther&& other) {
        using TRaw = std::decay_t<TOther>;
        storage.template get<TRaw>() = std::forward<TOther>(other);
    }
};

/// \brief Creates a variant by copying/moving value currently stored in other variant.
template <typename... TArgs>
struct CopyMoveAssign {
    AlignedUnion<TArgs...>& storage;

    template <typename T, typename TOther>
    void visit(const TOther& other) {
        storage.template get<T>() = other.template get<T>();
    }

    template <typename T,
        typename TOther,
        typename = std::enable_if_t<!std::is_lvalue_reference<TOther>::value>>
    void visit(TOther&& other) {
        storage.template get<T>() = std::move(other.template get<T>());
    }
};

/// \brief Swaps content of two variants.
template <typename... TArgs>
struct Swap {
    AlignedUnion<TArgs...>& storage;

    template <typename T, typename TOther>
    void visit(TOther&& other) {
        std::swap(storage.template get<T>(), other.template get<T>());
    }
};

} // namespace VariantHelpers

/// \brief Iterates through types of variant until idx-th type is found, and executes given TVisitor, passing
/// arguments to its method \ref visit.
template <typename T0, typename... TArgs>
struct VariantIterator {
    template <typename TVisitor, typename... Ts>
    static void visit(Size idx, TVisitor&& visitor, Ts&&... args) {
        if (idx == 0) {
            visitor.template visit<T0>(std::forward<Ts>(args)...);
        } else {
            VariantIterator<TArgs...>::visit(
                idx - 1, std::forward<TVisitor>(visitor), std::forward<Ts>(args)...);
        }
    }
};
/// \brief Specialization for last type
template <typename T0>
struct VariantIterator<T0> {
    template <typename TVisitor, typename... Ts>
    static void visit(Size UNUSED_IN_RELEASE(idx), TVisitor&& visitor, Ts&&... args) {
        ASSERT(idx == 0);
        visitor.template visit<T0>(std::forward<Ts>(args)...);
    }
};

/// \brief Tag for invoking constructor with type index as parameter.
static constexpr struct ConstructTypeIdxTag {
} CONSTRUCT_TYPE_IDX;

/// \brief Variant, an implementation of type-safe union, similar to std::variant or boost::variant.
template <typename... TArgs>
class Variant {
private:
    AlignedUnion<TArgs...> storage;
    Size typeIdx;

    void destroy() {
        VariantHelpers::Delete<TArgs...> deleter{ storage };
        VariantIterator<TArgs...>::visit(typeIdx, deleter);
    }

    static_assert(sizeof...(TArgs) >= 1, "Must have at least 1 variant");

public:
    /// \brief Default constructor, constructs the object of type listed first in the type list using its
    /// default constructor.
    ///
    /// The default constructor must exist.
    Variant() {
        using T = SelectType<0, TArgs...>;
        storage.template emplace<T>();
        typeIdx = 0;
    }

    /// \brief Construct variant from value of stored type
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    Variant(T&& value) {
        using RawT = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        storage.template emplace<RawT>(std::forward<T>(value));
        typeIdx = idx;
    }

    /// \brief Default-constructs a type with given type index.
    ///
    /// Useful for (de)serialization of variant.
    Variant(const ConstructTypeIdxTag, const Size typeIdx)
        : typeIdx(typeIdx) {
        ASSERT(typeIdx < sizeof...(TArgs));
        VariantHelpers::DefaultCreate<TArgs...> creator{ storage };
        VariantIterator<TArgs...>::visit(typeIdx, creator);
    }

    Variant(const Variant& other) {
        VariantHelpers::CopyMoveCreate<TArgs...> creator{ storage };
        VariantIterator<TArgs...>::visit(other.typeIdx, creator, other);
        typeIdx = other.typeIdx;
    }

    Variant(Variant&& other) {
        VariantHelpers::CopyMoveCreate<TArgs...> creator{ storage };
        VariantIterator<TArgs...>::visit(other.typeIdx, creator, std::move(other));
        typeIdx = other.typeIdx;
    }

    ~Variant() {
        destroy();
    }

    /// \brief Universal copy/move operator with type of rhs being one of stored types.
    ///
    /// Valid type is checked by static assert. If the type of rhs is the same as the type currently stored in
    /// variant, the value is copy/move assigned, otherwise the current value is destroyed and a copy/move
    /// constructor is used.
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    Variant& operator=(T&& value) {
        using RawT = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        if (int(typeIdx) != idx) {
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

    /// \brief Assigns another variant into this.
    ///
    /// If the type of the currently stored value is the same as type in rhs, the copy operator is utilized,
    /// otherwise the current value (if there is one) is destroyed and a new value is created using copy
    /// constructor.
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

    /// \brief Moves another variant into this.
    ///
    /// If the type of the currently stored value is the same as type in rhs, the move operator is utilized,
    /// otherwise the current value is destroyed and a new value is created using move constructor.
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
        return *this;
    }

    /// \brief Swaps value stored in the variant with value of different variant.
    ///
    /// Both variants must hold the value of the same type, checked by assert.
    void swap(Variant& other) {
        ASSERT(typeIdx == other.typeIdx);
        VariantHelpers::Swap<TArgs...> swapper{ storage };
        VariantIterator<TArgs...>::visit(typeIdx, swapper, other);
    }

    /// \brief Creates a value of type T in place.
    ///
    /// Type T must be one of variant types. Any previously stored value is destroyed.
    template <typename T, typename... Ts>
    void emplace(Ts&&... args) {
        using RawT = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        destroy();
        storage.template emplace<RawT>(std::forward<Ts>(args)...);
        typeIdx = idx;
    }

    /// \brief Returns index of type currently stored in variant.
    INLINE Size getTypeIdx() const {
        return typeIdx;
    }

    /// \brief Checks if the variant currently hold value of given type.
    template <typename T>
    INLINE bool has() const {
        constexpr int idx = getTypeIndex<std::decay_t<T>, TArgs...>;
        return typeIdx == idx;
    }

    /// \brief Checks if the given type is one of the listed ones.
    ///
    /// Returns true even for references and const/volatile-qualified types.
    template <typename T>
    INLINE static constexpr bool canHold() {
        using RawT = std::decay_t<T>;
        return getTypeIndex<RawT, TArgs...> != -1;
    }

    /// \brief Returns the stored value.
    ///
    /// Performs a compile-time check that the type is contained in Variant, and runtime check that the
    /// variant currently holds value of given type.
    template <typename T>
    INLINE T& get() {
        constexpr int idx = getTypeIndex<std::decay_t<T>, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT(typeIdx == idx);
        return storage.template get<T>();
    }

    /// \brief Returns the stored value, const version.
    template <typename T>
    INLINE const T& get() const {
        constexpr int idx = getTypeIndex<std::decay_t<T>, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT(typeIdx == idx);
        return storage.template get<T>();
    }

    /// \brief Implicit conversion to one of stored values.
    ///
    /// Performs a compile-time check that the type is contained in Variant, and runtime check that the
    /// variant currently holds value of given type.
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    operator T&() {
        return get<T>();
    }

    /// \brief Const version of conversion operator.
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    operator const T&() const {
        return get<T>();
    }

    /// \brief Returns the stored value in the variant.
    ///
    /// Safer than implicit conversion as it returns NOTHING in case the value is currently not stored in
    /// variant.
    template <typename T>
    Optional<T> tryGet() const {
        constexpr int idx = getTypeIndex<std::decay_t<T>, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        if (typeIdx != getTypeIndex<std::decay_t<T>, TArgs...>) {
            return NOTHING;
        }
        return storage.template get<T>();
    }
};


namespace Detail {
template <Size N, typename T0, typename... TArgs>
struct ForValue {
    template <typename TVariant, typename TFunctor>
    INLINE static decltype(auto) action(TVariant&& variant, TFunctor&& functor) {
        if (variant.getTypeIdx() == N) {
            return functor(variant.template get<T0>());
        } else {
            return ForValue<N + 1, TArgs...>::action(
                std::forward<TVariant>(variant), std::forward<TFunctor>(functor));
        }
    }
};

// specialization for NothingType, does not call the functor
template <Size N, typename... TArgs>
struct ForValue<N, NothingType, TArgs...> {
    template <typename TVariant, typename TFunctor>
    INLINE static decltype(auto) action(TVariant&& variant, TFunctor&& functor) {
        if (variant.getTypeIdx() != N) {
            return ForValue<N + 1, TArgs...>::action(
                std::forward<TVariant>(variant), std::forward<TFunctor>(functor));
        }
        STOP;
    }
};

template <Size N, typename T0>
struct ForValue<N, T0> {
    template <typename TVariant, typename TFunctor>
    INLINE static decltype(auto) action(TVariant&& variant, TFunctor&& functor) {
        return functor(variant.template get<T0>());
    }
};
} // namespace Detail

/// Executes a functor passing current value stored in variant as its parameter. The functor must either have
/// templated operator() (i.e. generic lambda), or have an overloaded operator for each type contained in
/// variant.
/// If the variant contains type NothingType, the functor not called for this type. This allows to call
/// generic functions, defined on other types while keeping NothingType as a special value representing an
/// empty variant.
/// \param variant Variant, must contain a value (checked by assert).
/// \param functor Executed functor, takes one parameter - value obtained from Variant.
/// \return Value returned by the functor.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) forValue(Variant<TArgs...>& variant, TFunctor&& functor) {
    return Detail::ForValue<0, TArgs...>::action(variant, std::forward<TFunctor>(functor));
}

/// Executes a functor passing current value stored in variant as its parameter. Overload for const l-value
/// reference.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) forValue(const Variant<TArgs...>& variant, TFunctor&& functor) {
    return Detail::ForValue<0, TArgs...>::action(variant, std::forward<TFunctor>(functor));
}

NAMESPACE_SPH_END

/// Overload of std::swap for Sph::Variant.
namespace std {
template <typename... TArgs>
void swap(Sph::Variant<TArgs...>& v1, Sph::Variant<TArgs...>& v2) {
    v1.swap(v2);
}
} // namespace std
