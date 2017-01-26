#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/LinkedList.h"
#include "objects/finders/Voxel.h"
#include "objects/wrappers/Range.h"
#include "sph/initial/Distribution.h"
#include "geometry/Domain.h"

using namespace Sph;

void testFinder(Abstract::Finder& finder) {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(50, domain);

    finder.build(storage);

    Array<NeighbourRecord> treeNeighs;
    Size nTree = finder.findNeighbours(25, 1.5_f, treeNeighs);

    /// checksum - count by bruteforce number of neighbours

    Array<Size> bfNeighs;

    for (Size i = 0; i < storage.size(); ++i) {
        if (getSqrLength(storage[25] - storage[i]) <= sqr(1.5_f)) {
            bfNeighs.push(i);
        }
    }

    REQUIRE(nTree == bfNeighs.size());

    // sort both indices and check pair by pair
    std::sort(treeNeighs.begin(), treeNeighs.end(), [](NeighbourRecord n1, NeighbourRecord n2) {
        return n1.index < n2.index;
    });
    std::sort(bfNeighs.begin(), bfNeighs.end());

    for (Size i = 0; i < nTree; ++i) {
        REQUIRE(bfNeighs[i] == treeNeighs[i].index);
        REQUIRE(treeNeighs[i].distanceSqr == getSqrLength(storage[treeNeighs[i].index] - storage[25]));
    }
}

void testFinderSmallerH(Abstract::Finder& finder) {
    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i)); // points on line with increasing H
    }

    finder.build(storage);
    Array<NeighbourRecord> treeNeighs;
    int nAll = finder.findNeighbours(4, 10._f, treeNeighs);
    REQUIRE(nAll == 10); // this should find all particles

    int nSmaller = finder.findNeighbours(4, 10._f, treeNeighs, FinderFlags::FIND_ONLY_SMALLER_H);
    REQUIRE(nSmaller == 4); // this should find indices 0, 1, 2, 3
    bool allMatching = true;
    for (auto& n : treeNeighs) {
        if (n.index > 3) {
            allMatching = false;
        }
    }
    REQUIRE(allMatching);
}

TEST_CASE("KdTree", "[finders]") {
    KdTree finder;
    testFinder(finder);
    testFinderSmallerH(finder);
}

/*TEST_CASE("LinkedList", "[finders]") {
    LinkedList linkedList;
    testFinder(linkedList);
    testFinderSmallerH(linkedList);
}*/

TEST_CASE("VoxelFinder", "[finders]") {
    VoxelFinder finder;
    testFinder(finder);
    testFinderSmallerH(finder);
}
