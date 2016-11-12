#pragma once

/// Object capable of storing values of different time, having only one value (and one type) in any moment.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"
#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN


/// Executes a functor with the current value (and current type) as a parameter
template <int n, typename T0, typename... TArgs>
struct ForCurrentType {
    template <typename TFunctor>
    static void action(const int idx, const char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<const T0*>(data));
        } else {
            ForCurrentType<n + 1, TArgs...>::action(idx, data, std::forward<TFunctor>(functor));
        }
    }

    template <typename TFunctor>
    static void action(const int idx, char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<T0*>(data));
        } else {
            ForCurrentType<n + 1, TArgs...>::action(idx, data, std::forward<TFunctor>(functor));
        }
    }
};
template <int n, typename T0>
struct ForCurrentType<n, T0> {
    template <typename TFunctor>
    static void action(const int idx, const char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<const T0*>(data));
        }
    }

    template <typename TFunctor>
    static void action(const int idx, char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<T0*>(data));
        }
    }
};


template <typename... TArgs>
class AlignedUnion : public Object {
private:
    std::aligned_union_t<0, TArgs...> storage;

public:
    template <typename T>
    T& get() {
        return *reinterpret_cast<T*>(&storage);
    }

    template <typename T>
    const T& get() const {
        return *reinterpret_cast<const T*>(&storage);
    }
};

namespace VariantHelpers {
    template <typename... TArgs>
    struct Create {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            using TRaw = std::decay_t<TOther>;
            new (&storage) TRaw(std::forward<TOther>(other));
        }
    };

    template <typename... TArgs>
    struct Delete {
        AlignedUnion<TArgs...>& storage;

        template <typename T>
        void visit() {
            storage.template get<T>().~T();
        }
    };

    template <typename... TArgs>
    struct CopyMoveCreate {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            if (std::is_lvalue_reference<TOther>::value) {
                new (&storage) T(other.template operator T());
            } else {
                new (&storage) T(std::move(other.template operator T()));
            }
        }
    };

    template <typename... TArgs>
    struct Assign {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            using TRaw = std::decay_t<TOther>;
            storage.template get<TRaw>() = std::forward<TOther>(other);
        }
    };

    template <typename... TArgs>
    struct CopyMoveAssign {
        AlignedUnion<TArgs...>& storage;

        template <typename T, typename TOther>
        void visit(TOther&& other) {
            if (std::is_lvalue_reference<TOther>::value) {
                storage.template get<T>() = other.template operator T();
            } else {
                storage.template get<T>() = std::move(other.template operator T());
            }
        }
    };
}


template <typename T0, typename... TArgs>
struct VariantIterator {
    template <typename TVisitor, typename... Ts>
    static void visit(int idx, TVisitor&& visitor, Ts&&... args) {
        if (idx == 0) {
            visitor.template visit<T0>(std::forward<Ts>(args)...);
        } else {
            VariantIterator<TArgs...>::visit(idx - 1,
                                             std::forward<TVisitor>(visitor),
                                             std::forward<Ts>(args)...);
        }
    }
};

// specialization for last type
template <typename T0>
struct VariantIterator<T0> {
    template <typename TVisitor, typename... Ts>
    static void visit(int idx, TVisitor&& visitor, Ts&&... args) {
        ASSERT(idx == 0);
        visitor.template visit<T0>(std::forward<Ts>(args)...);
    }
};


/// Variant, an implementation of type-safe union
template <typename... TArgs>
class Variant : public Object {
private:
    AlignedUnion<TArgs...> storage;
    int typeIdx = -1;

    template <typename T>
    T& getUnchecked() {
        return storage.get<T>();
    }

    template <typename T>
    const T& getUnchecked() const {
        return storage.get<T>();
    }
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
        using RawT        = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        VariantHelpers::Create<TArgs...> creator{ storage };
        VariantIterator<TArgs...>::visit(idx, creator, std::forward<T>(value));
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
        typeIdx = other.typeIdx;
    }

    /// Universal copy/move operator with type of rhs being one of stored types
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value>>
    Variant& operator=(T&& value) {
        using RawT        = std::decay_t<T>;
        constexpr int idx = getTypeIndex<RawT, TArgs...>;
        static_assert(idx != -1, "Type must be listed in Variant");
        if (typeIdx != idx) {
            destroy();
        }
        VariantHelpers::Assign<TArgs...> assigner{ storage };
        VariantIterator<TArgs...>::visit(idx, assigner, std::forward<T>(value));
        typeIdx = idx;
        return *this;
    }

    Variant& operator=(const Variant& other) {
        if (typeIdx != other.typeIdx) {
            destroy();
        }
        VariantHelpers::CopyMoveAssign<TArgs...> assigner{ storage };
        VariantIterator<TArgs...>::visit(other.typeIdx, assigner, other);
        typeIdx = other.typeIdx;
        return *this;
    }

    Variant& operator=(Variant&& other) {
        if (typeIdx != other.typeIdx) {
            destroy();
        }
        VariantHelpers::CopyMoveAssign<TArgs...> assigner{ storage };
        VariantIterator<TArgs...>::visit(other.typeIdx, assigner, std::move(other));
        typeIdx = other.typeIdx;
        return *this;
    }

    int getTypeIdx() const { return typeIdx; }

    /// Implicit conversion to one of stored values. Performs a compile-time check that the type is contained
    /// in Variant, and runtime check that the variant currently holds value of given type.
    template <typename T>
    operator T&() {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT((typeIdx == getTypeIndex<T, TArgs...>));
        return getUnchecked<T>();
    }

    /// const version
    template <typename T>
    operator T() const {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT((typeIdx == getTypeIndex<T, TArgs...>));
        return getUnchecked<T>();
    }


    /// Returns the stored value in the variant. Safer than implicit conversion as it returns NOTHING in case
    /// the value is currently not stored in variant.
    template <typename T>
    Optional<T> get() const {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        if (typeIdx != getTypeIndex<T, TArgs...>) {
            return NOTHING;
        }
        return getUnchecked<T>();
        ;
    }
};

NAMESPACE_SPH_END
