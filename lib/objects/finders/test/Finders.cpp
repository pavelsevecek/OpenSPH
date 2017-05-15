#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/LinkedList.h"
#include "objects/finders/Octree.h"
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


    auto test = [&](const Size refIdx) -> Outcome {
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
        Array<Size> bfIdxs, treeIdxs;
        Array<Float> bfDist, treeDist;
        for (auto& n : bfNeighs) {
            bfIdxs.push(n.index);
            bfDist.push(n.distanceSqr);
        }
        for (auto& n : treeNeighs) {
            treeIdxs.push(n.index);
            treeDist.push(n.distanceSqr);
        }

        if (!(bfIdxs == treeIdxs)) {
            return makeFailed(
                "Different neighbours found:", "\n brute force: ", bfIdxs, "\n finder: ", treeIdxs);
        }
        for (Size i = 0; i < bfDist.size(); ++i) {
            if (bfDist[i] != approx(treeDist[i])) { /// \todo figure out why approx is needed here!
                return makeFailed(
                    "Neighbours are at different distances!"
                    "\n brute force: ",
                    bfDist[i],
                    "\n finder: ",
                    treeDist[i]);
            }
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, storage.size());
}

void testFinderSmallerH(Abstract::Finder& finder) {
    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i + 1)); // points on line with increasing H
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

TEST_CASE("Octree", "[finders]") {
    Octree finder;
    testFinder(finder, EMPTY_FLAGS);
    testFinder(finder, FinderFlags::FIND_ONLY_SMALLER_H);
    testFinderSmallerH(finder);

    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(1000, domain);
    finder.build(storage);
    Size cnt = 0;
    finder.enumerateChildren([&cnt](OctreeNode& node) { cnt += node.points.size(); });
    REQUIRE(cnt == storage.size());
}
