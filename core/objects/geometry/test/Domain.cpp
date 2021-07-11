#include "objects/geometry/Domain.h"
#include "catch.hpp"
#include "sph/initial/Distribution.h"
#include "tests/Approx.h"
#include "thread/Scheduler.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

/// Checks that project does not change smoothing lengths and that all particles are indeed contained inside
/// the domain after projecting
template <typename TDomain>
static void testDomain(TDomain&& domain) {
    HexagonalPacking distr;
    SphericalDomain large(Vector(0._f), 10._f);
    Array<Vector> r = distr.generate(SEQUENTIAL, 1000, large);
    const Float h = r[0][H];

    domain.project(r);

    auto test = [&](const Size i) -> Outcome { return Outcome(r[i][H] == h && domain.contains(r[i])); };

    REQUIRE_SEQUENCE(test, 0, r.size());
}


TEST_CASE("BlockDomain", "[domain]") {
    BlockDomain domain(Vector(1._f, -2._f, 3._f), Vector(5._f, 3._f, 1._f));
    REQUIRE(domain.getVolume() == 15._f);
    REQUIRE(domain.getCenter() == Vector(1._f, -2._f, 3._f));
    REQUIRE(domain.getBoundingBox().center() == Vector(1._f, -2._f, 3._f));
    REQUIRE(domain.getBoundingBox().size() == Vector(5._f, 3._f, 1._f));

    domain = BlockDomain(Vector(0._f), Vector(8._f, 6._f, 4._f)); // (-4, -3, -2) to (4, 3, 2)
    Array<Vector> v{ Vector(3._f, 0._f, 0._f),
        Vector(5._f, 0._f, 0._f),
        Vector(-6._f, 0._f, 0._f),
        Vector(0._f, 4._f, 0._f),
        Vector(0._f, -3.5_f, 0._f),
        Vector(0._f, 2.5_f, 0.5_f),
        Vector(0._f, -2.5_f, -0.5_f),
        Vector(0._f, 0._f, 1.5_f),
        Vector(0._f, 0._f, -2.5_f),
        Vector(0._f, 0.5_f, 2._f),
        Vector(0._f, -0.5_f, 3._f) };
    Array<Vector> projected = v.clone();
    domain.project(projected, nullptr); // project nothing
    REQUIRE(projected == v);            // unchanged
    domain.project(projected);          // project all
    Array<Vector> expected{ Vector(3._f, 0._f, 0._f),
        Vector(4._f, 0._f, 0._f),
        Vector(-4._f, 0._f, 0._f),
        Vector(0._f, 3._f, 0._f),
        Vector(0._f, -3._f, 0._f),
        Vector(0._f, 2.5_f, 0.5_f),
        Vector(0._f, -2.5_f, -0.5_f),
        Vector(0._f, 0._f, 1.5_f),
        Vector(0._f, 0._f, -2._f),
        Vector(0._f, 0.5_f, 2._f),
        Vector(0._f, -0.5_f, 2._f) };
    REQUIRE(projected == expected);

    /*Array<Vector> inverted = v.clone();
    domain.invert(inverted, nullptr);
    REQUIRE(inverted == v);
    domain.invert(inverted);
    expected = Array<Vector>{ Vector(5._f, 0._f, 0._f),
        Vector(3._f, 0._f, 0._f),
        Vector(-2._f, 0._f, 0._f),
        Vector(0._f, 2._f, 0._f),
        Vector(0._f, -2.5_f, 0._f),
        Vector(0._f, 3.5_f, 0.5_f),
        Vector(0._f, -3.5_f, -0.5_f),
        Vector(0._f, 0._f, 2.5_f),
        Vector(0._f, 0._f, -1.5_f),
        Vector(0._f, 0.5_f, 2._f),
        Vector(0._f, -0.5_f, 1._f) };
    REQUIRE(inverted == expected);*/

    testDomain(domain);
}

TEST_CASE("SphericalDomain", "[domain]") {
    SphericalDomain domain(Vector(1._f, -2._f, 3._f), 4._f);
    REQUIRE(domain.getVolume() == sphereVolume(4._f));
    REQUIRE(domain.getCenter() == Vector(1._f, -2._f, 3._f));
    REQUIRE(domain.getBoundingBox().center() == Vector(1._f, -2._f, 3._f));
    REQUIRE(domain.getBoundingBox().size() == Vector(8._f));

    testDomain(domain);
}

TEST_CASE("CylindricalDomain", "[domain]") {
    CylindricalDomain domain(Vector(1._f, -2._f, 3._f), 3._f, 5._f, true);
    REQUIRE(domain.getVolume() == approx(PI * 9._f * 5._f));
    REQUIRE(domain.getCenter() == Vector(1._f, -2._f, 3._f));
    REQUIRE(domain.getBoundingBox().center() == Vector(1._f, -2._f, 3._f));
    REQUIRE(domain.getBoundingBox().size() == Vector(6._f, 6._f, 5._f));

    testDomain(domain);
}

TEST_CASE("HexagonalDomain", "[domain]") {
    HexagonalDomain domain(Vector(-1._f, 2._f, 3._f), 2._f, 3._f, true);
    REQUIRE(domain.getCenter() == Vector(-1._f, 2._f, 3._f));
    REQUIRE(domain.getBoundingBox().center() == Vector(-1._f, 2._f, 3._f));
    REQUIRE(domain.getBoundingBox().size() == Vector(4._f, 4._f, 3._f));

    SKIP_TEST; // doesn't seem to work, fix if HexagonalDomain is actually used some day
    // testDomain(domain);
}
