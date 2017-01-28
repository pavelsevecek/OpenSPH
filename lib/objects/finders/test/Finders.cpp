#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/LinkedList.h"
#include "objects/finders/Voxel.h"
#include "objects/wrappers/Range.h"
#include "sph/initial/Distribution.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

void testFinder(Abstract::Finder& finder, Flags<FinderFlags> flags) {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(1000, domain);
    finder.build(storage);

    BruteForceFinder bf;
    bf.build(storage);

    Array<NeighbourRecord> treeNeighs;
    Array<NeighbourRecord> bfNeighs;
    const Float radius = 0.7_f;


    auto test = [&](const Size refIdx) {
        const Size nTree = finder.findNeighbours(refIdx, radius, treeNeighs, flags);
        const Size nBf = bf.findNeighbours(refIdx, radius, bfNeighs, flags);

        if (nTree != nBf) {
            return makeFailed("Invalid number of neighbours:\n", nTree, " == ", nBf);
        }

        // sort both indices and check pair by pair
        std::sort(treeNeighs.begin(), treeNeighs.end(), [](NeighbourRecord n1, NeighbourRecord n2) {
            return n1.index < n2.index;
        });
        std::sort(bfNeighs.begin(), bfNeighs.end(), [](NeighbourRecord n1, NeighbourRecord n2) {
            return n1.index < n2.index;
        });

        if (!(bfNeighs == treeNeighs)) {
            return makeFailed("Different neighbours found");
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, storage.size());
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
    testFinder(finder, EMPTY_FLAGS);
    testFinder(finder, FinderFlags::FIND_ONLY_SMALLER_H);
    testFinderSmallerH(finder);
}

/*TEST_CASE("LinkedList", "[finders]") {
    LinkedList linkedList;
    testFinder(linkedList);
    testFinderSmallerH(linkedList);
}*/

TEST_CASE("VoxelFinder", "[finders]") {
    VoxelFinder finder;
    testFinder(finder, EMPTY_FLAGS);
    testFinder(finder, FinderFlags::FIND_ONLY_SMALLER_H);
    testFinderSmallerH(finder);
}
