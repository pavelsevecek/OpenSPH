#include "objects/finders/PointCloud.h"
#include "catch.hpp"
#include "objects/finders/KdTree.h"
#include "objects/geometry/Domain.h"
#include "sph/initial/Distribution.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("PointCloud push", "[pointcloud]") {
    PointCloud pc(1);
    auto h1 = pc.push(Vector(1, 2, 3));
    REQUIRE(pc.size() == 1);
    REQUIRE(pc.point(h1) == Vector(1, 2, 3));

    auto h2 = pc.push(Vector(4, 5, 6));
    REQUIRE(pc.size() == 2);
    REQUIRE(pc.point(h2) == Vector(4, 5, 6));
    REQUIRE(pc.point(h1) == Vector(1, 2, 3));

    auto h3 = pc.push(pc.point(h1));
    REQUIRE(pc.size() == 3);
    REQUIRE(h1 != h3);
    REQUIRE(pc.point(h1) == pc.point(h3));
}

TEST_CASE("PointCloud find close", "[pointcloud]") {
    RandomDistribution dist(1234);
    SphericalDomain domain(Vector(0._f), 3._f);
    Array<Vector> points = dist.generate(SEQUENTIAL, 10000, domain);

    PointCloud pc(0.25_f);
    pc.push(points);

    KdTree<KdNode> tree;
    tree.build(SEQUENTIAL, points, FinderFlag::SKIP_RANK);

    auto test = [&](const Size i) -> Outcome {
        Array<NeighbourRecord> idxs;
        tree.findAll(points[i], 0.2_f, idxs);
        Array<Vector> neighs1;
        for (const auto& n : idxs) {
            neighs1.push(points[n.index]);
        }

        Array<Vector> neighs2;
        pc.findClosePoints(points[i], 0.2_f, neighs2);

        std::sort(neighs1.begin(), neighs1.end(), lexicographicalLess);
        std::sort(neighs2.begin(), neighs2.end(), lexicographicalLess);

        if (neighs2.size() != neighs1.size()) {
            pc.findClosePoints(points[i], 0.2_f, neighs2);
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
