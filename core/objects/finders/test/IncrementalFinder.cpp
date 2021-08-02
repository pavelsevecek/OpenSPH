#include "objects/finders/IncrementalFinder.h"
#include "catch.hpp"
#include "objects/finders/KdTree.h"
#include "objects/geometry/Domain.h"
#include "sph/initial/Distribution.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("IncrementalFinder addPoint", "[incrementalfinder]") {
    IncrementalFinder finder(1);
    auto h1 = finder.addPoint(Vector(1, 2, 3));
    REQUIRE(finder.size() == 1);
    REQUIRE(finder.point(h1) == Vector(1, 2, 3));

    auto h2 = finder.addPoint(Vector(4, 5, 6));
    REQUIRE(finder.size() == 2);
    REQUIRE(finder.point(h2) == Vector(4, 5, 6));
    REQUIRE(finder.point(h1) == Vector(1, 2, 3));

    auto h3 = finder.addPoint(finder.point(h1));
    REQUIRE(finder.size() == 3);
    REQUIRE(h1 != h3);
    REQUIRE(finder.point(h1) == finder.point(h3));
}

TEST_CASE("IncrementalFinder findAll", "[incrementalfinder]") {
    RandomDistribution dist(1234);
    SphericalDomain domain(Vector(0._f), 3._f);
    Array<Vector> points = dist.generate(SEQUENTIAL, 10000, domain);

    IncrementalFinder finder(0.25_f);
    finder.addPoints(points);

    KdTree<KdNode> tree;
    tree.build(SEQUENTIAL, points, FinderFlag::SKIP_RANK);

    auto test = [&](const Size i) -> Outcome {
        Array<NeighborRecord> idxs;
        tree.findAll(points[i], 0.2_f, idxs);
        Array<Vector> neighs1;
        for (const auto& n : idxs) {
            neighs1.push(points[n.index]);
        }

        Array<Vector> neighs2;
        finder.findAll(points[i], 0.2_f, neighs2);

        std::sort(neighs1.begin(), neighs1.end(), lexicographicalLess);
        std::sort(neighs2.begin(), neighs2.end(), lexicographicalLess);

        if (neighs2.size() != neighs1.size()) {
            finder.findAll(points[i], 0.2_f, neighs2);
            return makeFailed("Different number of neighbors.\n", neighs1.size(), " == ", neighs2.size());
        }
        for (Size i = 0; i < neighs1.size(); ++i) {
            const Vector p1 = neighs1[i];
            const Vector p2 = neighs2[i];
            if (p1 != p2) {
                return makeFailed("Different neighbor found.\n", p1, " == ", p2);
            }
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, points.size());
}
