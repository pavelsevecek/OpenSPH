#pragma once

/// \file Object.h
/// \brief Common macros and basic objects.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include <cstdint>
#include <utility>

#define NAMESPACE_SPH_BEGIN namespace Sph {
#define NAMESPACE_SPH_END }

NAMESPACE_SPH_BEGIN

#define SPH_ARM

/// Macros for conditional compilation based on selected compiler
#ifdef __GNUC__
#ifdef __clang__
#define SPH_CLANG
#else
#define SPH_GCC
#endif
#endif
#ifdef _WIN32
#define SPH_WIN
#endif

#ifdef SPH_WIN
/// Turn on debug mode when compiling in debug configuration
#ifdef _DEBUG
#define SPH_DEBUG
#endif
#endif

/// Force inline
#ifdef SPH_DEBUG
#define INLINE inline
#define INL
#else
#ifdef SPH_WIN
#define INLINE __forceinline
#define INL
#else
#define INLINE __attribute__((always_inline)) inline
#define INL __attribute__((always_inline))
#endif
#endif

/// No inline
#ifdef SPH_WIN
#define NO_INLINE __declspec(noinline)
#else
#define NO_INLINE __attribute__((noinline))
#endif

#ifdef SPH_WIN
#define SPH_MAY_ALIAS
#else
#define SPH_MAY_ALIAS __attribute__((__may_alias__))
#endif

#define UNUSED(x)

/// \brief Silences the "unused variable" warning for given variable.
///
/// \note sizeof is used to make sure x is not evaluated.
#define MARK_USED(x) (void)sizeof(x)

#ifdef SPH_WIN
#define SPH_FALLTHROUGH
#else
#define SPH_FALLTHROUGH [[fallthrough]];
#endif

#define DEPRECATED [[deprecated]]

/// Branch prediction hints
#ifdef SPH_WIN
#define SPH_LIKELY(x) x
#define SPH_UNLIKELY(x) x
#else
#define SPH_LIKELY(x) __builtin_expect(bool(x), 1)
#define SPH_UNLIKELY(x) __builtin_expect(bool(x), 0)
#endif

/// Printing function names in assertions
#ifdef SPH_WIN
#define SPH_PRETTY_FUNCTION __FUNCSIG__
#else
#define SPH_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

/// \brief Object with deleted copy constructor and copy operator
struct Noncopyable {
    Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;
    Noncopyable(Noncopyable&&) = default;
    Noncopyable& operator=(const Noncopyable&) = delete;
    Noncopyable& operator=(Noncopyable&&) = default;
};

/// \brief Object with deleted copy and move constructor and copy and move operator
struct Immovable {
    Immovable() = default;

    Immovable(const Immovable&) = delete;
    Immovable(Immovable&&) = delete;
    Immovable& operator=(const Immovable&) = delete;
    Immovable& operator=(Immovable&&) = delete;
};


/// \brief Object intended to only be constructed on stack
///
/// \note This does not strictly enforce the object is constructed on stack, as we can easily wrap it to
/// another class and call operator new on the wrapper.
/// And yes, being local variable and being constructed on stack are not the same things. Try to think of a
/// better name if you can.
class Local {
    void* operator new(std::size_t) = delete;
    void* operator new[](std::size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

/// \brief Base class for all polymorphic objects
struct Polymorphic {
    virtual ~Polymorphic() {}
};

/// \brief Helper class used to allow calling a function only from within T.
template <typename T>
class Badge {
private:
    friend T;

    Badge() {}
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
} // namespace Detail

/// Static for loop from n1 to n2, including both values. Takes an object as an argument that must implement
/// templated method template<int n> visit(); for loop will pass the current index as a template parameter,
/// so that it can be used as a constant expression.
template <std::size_t N1, std::size_t N2, typename TVisitor>
INLINE void staticFor(TVisitor&& visitor) {
    Detail::StaticForType<N1, N2>::action(std::forward<TVisitor>(visitor));
}

namespace Detail {
/// \todo can be replaced with fold expressions
template <typename... TArgs>
void expandPack(TArgs&&...) {}
} // namespace Detail

template <typename TVisitor, typename... TArgs>
INLINE void staticForEach(TVisitor&& visitor, TArgs&&... args) {
    Detail::expandPack(visitor(std::forward<TArgs>(args))...);
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
} // namespace Detail

/// Returns N-th argument from an argument list. The type of the returned argument matches the type of the
/// input argument.
template <std::size_t N, typename TValue, typename... TOthers>
INLINE decltype(auto) selectNth(TValue&& value, TOthers&&... others) {
    return Detail::SelectNthType<0, N>::action(std::forward<TValue>(value), std::forward<TOthers>(others)...);
}

NAMESPACE_SPH_END
