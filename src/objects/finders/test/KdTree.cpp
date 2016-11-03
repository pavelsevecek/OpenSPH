#include "objects/finders/KdTree.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "sph/initconds/InitConds.h"

using namespace Sph;

TEST_CASE("KdTree", "[kdtree]") {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(50, &domain);

    KdTree tree;
    tree.build(storage);

    Array<NeighbourRecord> kdNeighs(0, 50);
    int nKdTree = tree.findNeighbours(25, 1._f, kdNeighs);

    /// checksum - count by bruteforce number of neighbours

    Array<int> bfNeighs(0, 50);

    for (int i = 0; i < storage.size(); ++i) {
        if (getSqrLength(storage[25] - storage[i]) <= 1._f) {
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
