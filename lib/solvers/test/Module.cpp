#include "solvers/Module.h"
#include "catch.hpp"

using namespace Sph;


TEST_CASE("Module", "[module]") {
    static_assert(!HasAccumulate<int>::value, "static test failed");
    struct DummyAccumulator {
        void accumulate(const int, const int, const Vector&) {}
    };
    static_assert(HasAccumulate<DummyAccumulator>::value, "static test failed");
}
