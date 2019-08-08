#include "thread/AtomicFloat.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "thread/Pool.h"

using namespace Sph;


TEST_CASE("AtomicFloat operations", "[thread]") {
    // just test that the operation are correctly defined (and do not assert nor throw),
    // this does not test atomicity of operators!
    AlignedStorage<Atomic<Float>> f1;
    REQUIRE_NOTHROW(f1.emplace());
    Atomic<Float> f2 = 2._f;
    REQUIRE(f2 == 2._f);
    f2 += 3._f;
    REQUIRE(f2 == 5._f);
    f2 -= 4._f;
    REQUIRE(f2 == 1._f);
    f2 *= 6._f;
    REQUIRE(f2 == 6._f);
    f2 /= 2._f;
    REQUIRE(f2 == 3._f);

    Atomic<Float> f3;
    f3 = f2 + 5._f;
    REQUIRE(f3 == 8._f);
    f3 = f2 - 1._f;
    REQUIRE(f3 == 2._f);
    f3 = f2 * 3._f;
    REQUIRE(f3 == 9._f);
    f3 = f2 / 3._f;
    REQUIRE(f3.get() == approx(1._f));
}

TEST_CASE("AtomicFloat comparison", "[thread]") {
    Atomic<Float> f1 = 3._f;
    REQUIRE(f1 == 3._f);
    REQUIRE_FALSE(f1 == 5._f);
    REQUIRE(f1 != 4._f);
    REQUIRE_FALSE(f1 != 3._f);
    REQUIRE(f1 > 2._f);
    REQUIRE_FALSE(f1 > 3._f);
    REQUIRE(f1 < 4._f);
    REQUIRE_FALSE(f1 < 2._f);
}

TEST_CASE("AtomicFloat concurrent addition", "[thread]") {
    ThreadPool pool;
    Atomic<Float> atomicSum = 0._f;
    Float sum = 0;
    for (Size i = 0; i <= 10000; ++i) {
        pool.submit([&sum, &atomicSum, i] {
            sum += Float(i);
            atomicSum += Float(i);
        });
    }
    const Float expected = 50'005'000._f; /// \todo ok for doubles, will this be precise even for floats?
    pool.waitForAll();
    REQUIRE(atomicSum == expected);
    REQUIRE(sum <= expected);
}
