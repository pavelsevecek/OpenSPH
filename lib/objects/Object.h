#pragma once

/// \file Object.h
/// \brief Common macros and basic objects.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include <stdint.h>
#include <utility>

#define NAMESPACE_SPH_BEGIN namespace Sph {
#define NAMESPACE_SPH_END }

NAMESPACE_SPH_BEGIN


/// Macros for conditional compilation based on selected compiler
#ifdef __GNUC__
#ifdef __clang__
#define SPH_CLANG
#else
#define SPH_GCC
#endif
#endif

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

#define MARK_USED(x) (void)x

#ifdef SPH_DEBUG
#define UNUSED_IN_RELEASE(x) x
#else
#define UNUSED_IN_RELEASE(x)
#endif

#define SPH_CPP17

#ifdef SPH_CPP17
#define SPH_FALLTHROUGH [[fallthrough]]
#else
#define SPH_FALLTHROUGH
#endif

#define DEPRECATED __attribute__((deprecated))

/// Branch prediction hints
#define SPH_LIKELY(x) __builtin_expect((x), 1)
#define SPH_UNLIKELY(x) __builtin_expect((x), 0)


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
