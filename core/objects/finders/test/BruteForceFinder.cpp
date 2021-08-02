#include "objects/finders/BruteForceFinder.h"
#include "catch.hpp"
#include "sph/initial/Distribution.h"
#include "thread/Pool.h"

using namespace Sph;

TEST_CASE("BruteForceFinder", "[finders]") {
    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i + 1)); // points on line with increasing H
    }

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    BruteForceFinder finder;
    finder.build(pool, storage);
    Array<NeighborRecord> neighs(0, 10);
    int cnt = finder.findAll(4, 1.5f, neighs);
    REQUIRE(cnt == 3);
    REQUIRE(neighs[0].index == 3);
    REQUIRE(neighs[0].distanceSqr == 1.f);
    REQUIRE(neighs[1].index == 4);
    REQUIRE(neighs[1].distanceSqr == 0.f);
    REQUIRE(neighs[2].index == 5);
    REQUIRE(neighs[2].distanceSqr == 1.f);

    cnt = finder.findLowerRank(4, 1.5f, neighs);
    REQUIRE(cnt == 1);
    REQUIRE(neighs[0].index == 3);
    REQUIRE(neighs[0].distanceSqr == 1.f);

    for (int i = 0; i < 10; ++i) {
        cnt = finder.findAll(0, i + 0.1f, neighs);
        REQUIRE(cnt == i + 1);
    }
}
