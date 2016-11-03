#include "objects/finders/BruteForce.h"
#include "catch.hpp"
#include "sph/initconds/InitConds.h"


using namespace Sph;

TEST_CASE("Bruteforce", "[bruteforce]") {
    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i)); // points on line with increasing H
    }

    BruteForceFinder finder;
    finder.build(storage);
    Array<NeighbourRecord> neighs(0, 10);
    int cnt = finder.findNeighbours(4, 1.5f, neighs, EMPTY_FLAGS);
    REQUIRE(cnt == 3);
    REQUIRE(neighs[0].index == 3);
    REQUIRE(neighs[0].distanceSqr == 1.f);
    REQUIRE(neighs[1].index == 4);
    REQUIRE(neighs[1].distanceSqr == 0.f);
    REQUIRE(neighs[2].index == 5);
    REQUIRE(neighs[2].distanceSqr == 1.f);

    cnt = finder.findNeighbours(4, 1.5f, neighs, FinderFlags::FIND_ONLY_SMALLER_H);
    REQUIRE(cnt == 1);
    REQUIRE(neighs[0].index == 3);
    REQUIRE(neighs[0].distanceSqr == 1.f);

    for (int i = 0; i < 10; ++i) {
        cnt = finder.findNeighbours(0, i + 0.1f, neighs);
        REQUIRE(cnt == i + 1);
    }
}
