#include "sph/initial/Distribution.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "system/ArrayStats.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "utils/Utils.h"

using namespace Sph;

void testDistribution(Abstract::Distribution* distribution) {
    BlockDomain domain(Vector(-3._f), Vector(2._f));
    Array<Vector> values = distribution->generate(1000, domain);
    REQUIRE(values.size() > 900);
    REQUIRE(values.size() < 1100);
    bool allInside = areAllMatching(values, [&](const Vector& v) { return domain.isInside(v); });
    REQUIRE(allInside);
}

TEST_CASE("HexaPacking", "[initconds]") {
    HexagonalPacking packing;
    testDistribution(&packing);
}

/*TEST_CASE("HexaPacking Benz&Asphaug", "[initconds]") {
    HexagonalPacking packing;
    SphericalDomain domain(Vector(0._f), 100._f);
    StdOutLogger logger;
    Array<Vector> r = packing.generate(100, domain);
    logger.write("Particles = ", r.size());

    TextOutput output("particles_%d.txt", "test", Array<QuantityIds>{ QuantityIds::POSITIONS });
    Storage storage;
    storage.emplace<Vector, OrderEnum::ZERO_ORDER>(QuantityIds::POSITIONS, std::move(r));
    output.dump(storage, 0._f);
}*/

TEST_CASE("HexaPacking sorted", "[initconds]") {
    HexagonalPacking sorted(HexagonalPacking::Options::SORTED);
    HexagonalPacking unsorted(EMPTY_FLAGS);

    BlockDomain domain(Vector(-3._f), Vector(2._f));
    Array<Vector> r_sort = sorted.generate(1000, domain);
    Array<Vector> r_unsort = unsorted.generate(1000, domain);
    ASSERT(r_sort.size() == r_unsort.size());


    std::unique_ptr<Abstract::Finder> finder_sort = Factory::getFinder(GLOBAL_SETTINGS);
    finder_sort->build(r_sort);
    std::unique_ptr<Abstract::Finder> finder_unsort = Factory::getFinder(GLOBAL_SETTINGS);
    finder_unsort->build(r_unsort);

    // find maximum distance of neighbouring particles in memory
    Size neighCnt_sort = 0;
    Size neighCnt_unsort = 0;
    Array<Size> dists_sort;
    Array<Size> dists_unsort;
    Array<NeighbourRecord> neighs;
    for (Size i = 0; i < r_sort.size(); ++i) {
        neighCnt_sort += finder_sort->findNeighbours(i, 2._f * r_sort[i][H], neighs);
        for (auto& n : neighs) {
            const Size dist = Size(abs(int(n.index) - int(i)));
            dists_sort.push(dist);
        }
    }
    for (Size i = 0; i < r_unsort.size(); ++i) {
        neighCnt_unsort += finder_unsort->findNeighbours(i, 2._f * r_unsort[i][H], neighs);
        for (auto& n : neighs) {
            const Size dist = Size(abs(int(n.index) - int(i)));
            dists_unsort.push(dist);
        }
    }

    // sanity check
    REQUIRE(neighCnt_sort == neighCnt_unsort);

    ArrayStats<Size> stats_sort(dists_sort);
    ArrayStats<Size> stats_unsort(dists_unsort);
    REQUIRE(stats_sort.median() < stats_unsort.median());
}

TEST_CASE("CubicPacking", "[initconds]") {
    CubicPacking packing;
    testDistribution(&packing);
}

TEST_CASE("RandomDistribution", "[initconds]") {
    RandomDistribution random;
    testDistribution(&random);
    // 100 points inside block [0,1]^d, approx. distance is 100^(-1/d)
}

TEST_CASE("LinearDistribution", "[initconds]") {
    LinearDistribution linear;
    SphericalDomain domain(Vector(0.5_f), 0.5_f);
    Array<Vector> values = linear.generate(101, domain);
    REQUIRE(values.size() == 101);
    bool equal = true;
    StdOutLogger logger;
    for (int i = 0; i <= 100; ++i) {
        if (!almostEqual(values[i], Vector(i / 100._f, 0._f, 0._f), 1.e-5_f)) {
            logger.write(values[i], " == ", Vector(i / 100._f, 0._f, 0._f));
            break;
            equal = false;
        }
    }
    REQUIRE(equal);
}
