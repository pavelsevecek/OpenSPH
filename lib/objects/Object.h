#pragma once

/// Common macros and basic objects.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include <stdint.h>
#include <utility>

#define NAMESPACE_SPH_BEGIN namespace Sph {
#define NAMESPACE_SPH_END }

NAMESPACE_SPH_BEGIN


/// Force inline for gcc
#ifdef SPH_DEBUG
#define INLINE inline
#define INL
#else
#define INLINE __attribute__((always_inline)) inline
#define INL __attribute__((always_inline))
#endif

#define NO_INLINE __attribute__((noinline))

#define UNUSED(x)

#ifdef SPH_DEBUG
#define UNUSED_IN_RELEASE(x) x
#else
#define UNUSED_IN_RELEASE(x)
#endif

#ifdef SPH_CPP17
#define FALLTHROUGH [[fallthrough]];
#else
#define FALLTHROUGH
#endif

#define DEPRECATED __attribute__((deprecated))

namespace Abstract {}

/// Object with deleted copy constructor and copy operator
struct Noncopyable {
    Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;

    Noncopyable(Noncopyable&&) = default;

    Noncopyable& operator=(const Noncopyable&) = delete;

    Noncopyable& operator=(Noncopyable&&) = default;
};

/// Base class for all polymorphic objects
struct Polymorphic {
    virtual ~Polymorphic() {}
};


namespace Detail {
    template <std::size_t N1, std::size_t N2>
    struct StaticForType {
        template <typename TVisitor>
        INLINE static void action(TVisitor&& visitor) {
            visitor.template visit<N1>();
            StaticForType<N1 + 1, N2>::action(std::forward<TVisitor>(visitor));
        }
    };
    template <std::size_t N>
    struct StaticForType<N, N> {
        template <typename TVisitor>
        INLINE static void action(TVisitor&& visitor) {
            visitor.template visit<N>();
        }
    };
}

/// Static for loop from n1 to n2, including both values. Takes an object as an argument that must implement
/// templated method template<int n> visit(); for loop will pass the current index as a template parameter,
/// so that it can be used as a constant expression.
template <std::size_t N1, std::size_t N2, typename TVisitor>
INLINE void staticFor(TVisitor&& visitor) {
    Detail::StaticForType<N1, N2>::action(std::forward<TVisitor>(visitor));
}


namespace Detail {
    template <std::size_t I, std::size_t N>
    struct SelectNthType {
        template <typename TValue, typename... TOthers>
        INLINE static decltype(auto) action(TValue&& UNUSED(value), TOthers&&... others) {
            return SelectNthType<I + 1, N>::action(std::forward<TOthers>(others)...);
        }
    };
    template <std::size_t N>
    struct SelectNthType<N, N> {
        template <typename TValue, typename... TOthers>
        INLINE static decltype(auto) action(TValue&& value, TOthers&&... UNUSED(others)) {
            return std::forward<TValue>(value);
        }
    };
}

/// Returns N-th argument from an argument list. The type of the returned argument matches the type of the
/// input argument.
template <std::size_t N, typename TValue, typename... TOthers>
INLINE decltype(auto) selectNth(TValue&& value, TOthers&&... others) {
    return Detail::SelectNthType<0, N>::action(std::forward<TValue>(value), std::forward<TOthers>(others)...);
}

NAMESPACE_SPH_END
