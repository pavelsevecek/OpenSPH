#include "solvers/Module.h"
#include "catch.hpp"

using namespace Sph;


TEST_CASE("Module traits", "[module]") {
    struct DummyModule {
        void accumulate(const int, const int, const Vector&) {}
        void update(Storage&) {}
        void integrate(Storage&) {}
        void initialize(Storage&, const BodySettings&) const {}
    };
    struct DummyAccumulator {
        void accumulate(const int, const int, const Vector&) {}
    };

    static_assert(!HasAccumulate<int>::value, "static test failed");
    static_assert(!HasUpdate<int>::value, "static test failed");
    static_assert(!HasIntegrate<int>::value, "static test failed");
    static_assert(!HasInitialize<int>::value, "static test failed");

    static_assert(HasAccumulate<DummyModule>::value, "static test failed");
    static_assert(HasUpdate<DummyModule>::value, "static test failed");
    static_assert(HasIntegrate<DummyModule>::value, "static test failed");
    static_assert(HasInitialize<DummyModule>::value, "static test failed");

    static_assert(HasAccumulate<DummyAccumulator>::value, "static test failed");
    static_assert(!HasUpdate<DummyAccumulator>::value, "static test failed");
    static_assert(!HasIntegrate<DummyAccumulator>::value, "static test failed");
    static_assert(!HasInitialize<DummyAccumulator>::value, "static test failed");
}
