#include "sph/initial/Distribution.h"
#include "catch.hpp"
#include "io/Output.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "system/ArrayStats.h"
#include "system/Factory.h"
#include "tests/Approx.h"
#include "thread/Scheduler.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

void testDistributionForDomain(IDistribution* distribution, const IDomain& domain) {
    Array<Vector> values = distribution->generate(SEQUENTIAL, 1000, domain);

    // distribution generates approximately 1000 particles
    REQUIRE(values.size() > 900);
    REQUIRE(values.size() < 1100);

    // all particles are inside prescribed domain
    bool allInside = areAllMatching(values, [&](const Vector& v) { return domain.contains(v); });
    REQUIRE(allInside);

    // if we split the cube to octants, each of them will have approximately the same number of particles.
    StaticArray<Size, 8> octants;
    octants.fill(0);
    for (Vector& v : values) {
        const Vector idx = 2._f * (v - domain.getBoundingBox().lower()) / domain.getBoundingBox().size();
        const Size octantIdx =
            4 * clamp(int(idx[X]), 0, 1) + 2 * clamp(int(idx[Y]), 0, 1) + clamp(int(idx[Z]), 0, 1);
        octants[octantIdx]++;
    }
    const Size n = values.size();
    for (Size o : octants) {
        REQUIRE(o >= n / 8 - 25);
        REQUIRE(o <= n / 8 + 25);
    }

    // check that all particles have the same smooting length
    const Float expectedH = root<3>(domain.getVolume() / n);
    auto test = [&](const Size i) -> Outcome {
        if (values[i][H] <= 0.8_f * expectedH || values[i][H] >= 1.2_f * expectedH) {
            return makeFailed("Invalid smoothing length: ", values[i][H], " == ", expectedH);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, n);
}

void testDistribution(IDistribution* distribution) {
    testDistributionForDomain(distribution, BlockDomain(Vector(-3._f), Vector(2._f)));
    testDistributionForDomain(distribution, CylindricalDomain(Vector(1._f, 2._f, 3._f), 2._f, 3._f, true));
    testDistributionForDomain(distribution, SphericalDomain(Vector(-2._f, 0._f, 1._f), 2.5_f));
}

TEST_CASE("HexaPacking common", "[initial]") {
    HexagonalPacking packing(EMPTY_FLAGS);
    testDistribution(&packing);
}

TEST_CASE("HexaPacking grid", "[initial]") {
    // test that within 1.5h of each particle, there are 12 neighbours in the same distance.
    HexagonalPacking packing(EMPTY_FLAGS);
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> r = packing.generate(SEQUENTIAL, 1000, domain);
    AutoPtr<ISymmetricFinder> finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(SEQUENTIAL, r);
    Array<NeighbourRecord> neighs;
    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 1.3_f) {
            // skip particles close to boundary, they don't necessarily have 12 neighbours
            return SUCCESS;
        }
        finder->findAll(i, 1.5_f * r[i][H], neighs);
        if (neighs.size() != 13) { // 12 + i-th particle itself
            return makeFailed("Invalid number of neighbours: \n", neighs.size(), " == 13");
        }
        const Float expectedDist =
            r[i][H]; // note that dist does not have to be exactly h, only approximately
        for (auto& n : neighs) {
            if (n.index == i) {
                continue;
            }
            const Float dist = getLength(r[i] - r[n.index]);
            if (dist != approx(expectedDist, 0.1_f)) {
                return makeFailed("Invalid distance to neighbours: \n", dist, " == ", expectedDist);
            }
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("HexaPacking sorted", "[initial]") {
    HexagonalPacking sorted(HexagonalPacking::Options::SORTED);
    HexagonalPacking unsorted(EMPTY_FLAGS);

    BlockDomain domain(Vector(-3._f), Vector(2._f));
    Array<Vector> r_sort = sorted.generate(SEQUENTIAL, 1000, domain);
    Array<Vector> r_unsort = unsorted.generate(SEQUENTIAL, 1000, domain);
    SPH_ASSERT(r_sort.size() == r_unsort.size());


    AutoPtr<ISymmetricFinder> finder_sort = Factory::getFinder(RunSettings::getDefaults());
    finder_sort->build(SEQUENTIAL, r_sort);
    AutoPtr<ISymmetricFinder> finder_unsort = Factory::getFinder(RunSettings::getDefaults());
    finder_unsort->build(SEQUENTIAL, r_unsort);

    // find maximum distance of neighbouring particles in memory
    Size neighCnt_sort = 0;
    Size neighCnt_unsort = 0;
    Array<Size> dists_sort;
    Array<Size> dists_unsort;
    Array<NeighbourRecord> neighs;
    for (Size i = 0; i < r_sort.size(); ++i) {
        neighCnt_sort += finder_sort->findAll(i, 2._f * r_sort[i][H], neighs);
        for (auto& n : neighs) {
            const Size dist = Size(abs(int(n.index) - int(i)));
            dists_sort.push(dist);
        }
    }
    for (Size i = 0; i < r_unsort.size(); ++i) {
        neighCnt_unsort += finder_unsort->findAll(i, 2._f * r_unsort[i][H], neighs);
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

TEST_CASE("CubicPacking", "[initial]") {
    CubicPacking packing;
    testDistribution(&packing);
}

TEST_CASE("RandomDistribution", "[initial]") {
    RandomDistribution random(123);
    testDistribution(&random);
    // 100 points inside block [0,1]^d, approx. distance is 100^(-1/d)
}

TEST_CASE("DiehlDistribution", "[initial]") {
    // Diehl et al. (2012) algorithm, using uniform particle density
    DiehlDistribution diehl(DiehlParams{});
    testDistribution(&diehl);
}

TEST_CASE("LinearDistribution", "[initial]") {
    LinearDistribution linear;
    SphericalDomain domain(Vector(0.5_f), 0.5_f);
    Array<Vector> values = linear.generate(SEQUENTIAL, 101, domain);
    REQUIRE(values.size() == 101);
    auto test = [&](const Size i) -> Outcome {
        if (values[i] != approx(Vector(i / 100._f, 0._f, 0._f), 1.e-5_f)) {
            return makeFailed(values[i], " == ", Vector(i / 100._f, 0._f, 0._f));
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, 100);
}
