#pragma once

/// Common macros and basic objects.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include <cassert>
#include <utility>
#include <stdint.h>

#define NAMESPACE_SPH_BEGIN namespace Sph {
#define NAMESPACE_SPH_END }

/// Turn off some warnings
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

NAMESPACE_SPH_BEGIN

#define DEBUG
#define PROFILE

#ifdef DEBUG
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

/// Helper macro marking missing implementation
#define NOT_IMPLEMENTED                                                                                      \
    ASSERT(false && "not implemented");                                                                      \
    throw std::exception();

/// Helper macro marking code that should never be executed (default branch of switch where there is finite
/// number of options, for example)
#define STOP                                                                                                 \
    ASSERT(false && "stop");                                                                                 \
    throw std::exception();

/// Force inline for gcc
#ifdef DEBUG
#define INLINE inline
#else
#define INLINE __attribute__((always_inline)) inline
#endif

#define NO_INLINE __attribute__((noinline))

#define UNUSED(x)

namespace Abstract {}

/// Object with deleted copy constructor and copy operator
class Noncopyable {
public:
    Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;

    Noncopyable(Noncopyable&&) = default;

    Noncopyable& operator=(const Noncopyable&) = delete;

    Noncopyable& operator=(Noncopyable&&) = default;
};

class Polymorphic  {
public:
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
    struct StaticForType<N, N>  {
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
        INLINE static decltype(auto) action(TValue&& value, TOthers&&... others) {
            return SelectNthType<I + 1, N>::action(std::forward<TOthers>(others)...);
        }
    };
    template <std::size_t N>
    struct SelectNthType<N, N> {
        template <typename TValue, typename... TOthers>
        INLINE static decltype(auto) action(TValue&& value, TOthers&&... others) {
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
