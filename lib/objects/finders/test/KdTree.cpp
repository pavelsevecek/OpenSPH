#include "objects/finders/KdTree.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "sph/distributions/Distribution.h"
#include "objects/wrappers/Range.h"
#include <iostream>

using namespace Sph;

TEST_CASE("KdTree", "[kdtree]") {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(50, &domain);

    KdTree tree;
    tree.build(storage);

    Array<NeighbourRecord> kdNeighs;
    int nKdTree = tree.findNeighbours(25, 1.5_f, kdNeighs);

    /// checksum - count by bruteforce number of neighbours

    Array<int> bfNeighs;

    for (int i = 0; i < storage.size(); ++i) {
        if (getSqrLength(storage[25] - storage[i]) <= Math::sqr(1.5_f)) {
            bfNeighs.push(i);
        }
    }

    REQUIRE(nKdTree == bfNeighs.size());

    // sort both indices and check pair by pair
    std::sort(kdNeighs.begin(), kdNeighs.end(), [](NeighbourRecord n1, NeighbourRecord n2) {
        return n1.index < n2.index;
    });
    std::sort(bfNeighs.begin(), bfNeighs.end());

    for (int i = 0; i < nKdTree; ++i) {
        REQUIRE(bfNeighs[i] == kdNeighs[i].index);
        REQUIRE(kdNeighs[i].distanceSqr == getSqrLength(storage[kdNeighs[i].index] - storage[25]));
    }
}

TEST_CASE("KdTree Smaller H", "[kdtree]") {
    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i)); // points on line with increasing H
    }

    KdTree tree;
    tree.build(storage);
    Array<NeighbourRecord> kdNeighs;
    int nAll = tree.findNeighbours(4, 10._f, kdNeighs);
    REQUIRE(nAll == 10); // this should find all particles

    int nSmaller = tree.findNeighbours(4, 10._f, kdNeighs, FinderFlags::FIND_ONLY_SMALLER_H);
    REQUIRE(nSmaller == 4); // this should find indices 0, 1, 2, 3
    bool allMatching = true;
    Range<int> range(0, 3);
    for (auto& n : kdNeighs) {
        if (!range.contains(n.index)) {
            allMatching = false;
        }
    }
    REQUIRE(allMatching);
}
