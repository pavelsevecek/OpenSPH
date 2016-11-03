#pragma once

/// Common macros and basic objects.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include <cassert>
#include <utility>

#define NAMESPACE_SPH_BEGIN namespace Sph {
#define NAMESPACE_SPH_END }

/// Turn off some warnings
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

NAMESPACE_SPH_BEGIN

#define DEBUG

#ifdef DEBUG
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

/// Force inline for gcc
#ifdef DEBUG
#define INLINE __attribute__((noinline)) inline
#else
#define INLINE __attribute__((always_inline)) inline
#endif

#define UNUSED(x)


namespace Abstract {}

/// Basic object from which all types are derived
class Object {};

/// Object with deleted copy constructor and copy operator
class Noncopyable : Object {
public:
    Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;

    Noncopyable& operator=(const Noncopyable&) = delete;
};

/// Dummy object that cannot be constructed, usable only in templates
class Unconstructible : public Object {
private:
    struct Tag {
        Tag() = delete;
    };

public:
    Unconstructible(Tag) {}
};

class Polymorphic : public Object {
public:
    virtual ~Polymorphic() {}
};


namespace Detail {
    template <int n1, int n2>
    struct StaticForType : public Object {
        template <typename TIteration>
        static void action(TIteration&& iteration) {
            iteration.template action<n1>();
            StaticForType<n1 + 1, n2>::action(iteration);
        }
    };
    template <int n>
    struct StaticForType<n, n> : public Object {
        template <typename TIteration>
        static void action(TIteration&& iteration) {
            iteration.template action<n>();
        }
    };
}

/// A static for loop from n1 to n2, including both values. Takes an object as an argument that must implement
/// templated method template<int n> action(); for loop will pass the current index as a template parameter,
/// so that it can be used as a constant expression.
template <int n1, int n2, typename TIteration>
void staticFor(TIteration&& iteration) {
    Detail::StaticForType<n1, n2>::action(std::forward<TIteration>(iteration));
}


NAMESPACE_SPH_END
