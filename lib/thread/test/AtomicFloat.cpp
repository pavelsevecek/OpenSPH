#include "thread/Pool.h"
#include "catch.hpp"
#include "thread/AtomicFloat.h"

using namespace Sph;


TEST_CASE("AtomicFloat", "[thread]") {
    ThreadPool pool;
    AtomicFloat atomicSum = 0._f;
    Float sum = 0;
    for (Size i = 0; i <= 100000; ++i) {
        pool.submit([&sum, i] { 
            sum += Float(i);
            atomicSum += Float(i);
        });
    }
    const Float expected = 50005000._f; /// \todo ok for doubles, will this be precise even for floats?
    pool.waitForAll();
    REQUIRE(atomicSum == expected);
    REQUIRE(sum < expected);
}
