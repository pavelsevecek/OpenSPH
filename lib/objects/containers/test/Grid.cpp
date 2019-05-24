#include "objects/containers/Grid.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("SparseGrid const", "[grid]") {
    const SparseGrid<RecordType> grid(4, RecordType(-1));

    RecordType::resetStats();
    REQUIRE(grid.size() == 4);
    REQUIRE(grid.voxelCount() == 64);
    REQUIRE_FALSE(grid.empty());
    REQUIRE(grid[Indices(0, 1, 2)] == -1);
    REQUIRE(grid[Indices(3, 3, 3)] == -1);
    REQUIRE(grid[Indices(2, 0, 0)] == -1);
    REQUIRE(RecordType::constructedNum == 3); // temporaries on the rhs

    REQUIRE_ASSERT(grid[Indices(4, 0, 1)]);
}

TEST_CASE("SparseGrid mutable", "[grid]") {
    SparseGrid<RecordType> grid(4, RecordType(-1));

    RecordType rhs(5);
    RecordType::resetStats();
    grid[Indices(2, 0, 1)] = rhs;
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(grid[Indices(2, 0, 1)] == rhs);
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(asConst(grid)[Indices(2, 0, 1)] == rhs);

    grid[Indices(0, 0, 0)] = 6;
    grid[Indices(3, 3, 3)] = 2;
    REQUIRE(grid[Indices(0, 0, 0)] == 6);
    REQUIRE(grid[Indices(2, 0, 1)] == 5);
    REQUIRE(grid[Indices(3, 3, 3)] == 2);
    REQUIRE(grid[Indices(2, 0, 0)] == -1);
    REQUIRE(grid[Indices(2, 0, 2)] == -1);
}

TEST_CASE("SparseGrid iterate", "[grid]") {
    SparseGrid<int> grid(4, 0);
    grid[Indices(1, 0, 0)] = 6;
    grid[Indices(3, 2, 2)] = 3;
    grid[Indices(2, 1, 0)] = 4;
    grid[Indices(1, 3, 0)] = 5;

    Array<Tuple<int, Indices>> expected = {
        { 6, Indices(1, 0, 0) },
        { 4, Indices(2, 1, 0) },
        { 5, Indices(1, 3, 0) },
        { 3, Indices(3, 2, 2) },
    };

    Size visitedCnt = 0;
    grid.iterate([&](int value, Indices idxs) {
        int expectedValue;
        Indices expectedIdxs;
        tieToTuple(expectedValue, expectedIdxs) = expected[visitedCnt];
        REQUIRE(value == expectedValue);
        REQUIRE(all(idxs == expectedIdxs));
        visitedCnt++;
    });
    REQUIRE(visitedCnt == 4);
}
