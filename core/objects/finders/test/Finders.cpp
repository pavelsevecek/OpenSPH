#include "catch.hpp"
#include "io/FileManager.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/HashMapFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Octree.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/Algorithm.h"
#include "quantities/Storage.h"
#include "run/Node.h"
#include "run/jobs/InitialConditionJobs.h"
#include "run/jobs/ParticleJobs.h"
#include "sph/initial/Distribution.h"
#include "system/Settings.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "thread/Tbb.h"
#include "utils/SequenceTest.h"

using namespace Sph;

static Outcome checkNeighborsEqual(ArrayView<NeighborRecord> treeNeighs, ArrayView<NeighborRecord> bfNeighs) {
    // sort both indices and check pair by pair
    std::sort(treeNeighs.begin(), treeNeighs.end(), [](NeighborRecord n1, NeighborRecord n2) {
        return n1.index < n2.index;
    });
    std::sort(bfNeighs.begin(), bfNeighs.end(), [](NeighborRecord n1, NeighborRecord n2) {
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
        return makeFailed("Different neighbors found:\n brute force: {}\n finder: {}", bfIdxs, treeIdxs);
    }
    for (Size i = 0; i < bfDist.size(); ++i) {
        if (bfDist[i] != approx(treeDist[i])) { /// \todo figure out why approx is needed here!
            return makeFailed("Neighbors are at different distances!\n brute force: {}\n finder: {}",
                bfDist[i],
                treeDist[i]);
        }
    }
    return SUCCESS;
}

static void checkNeighbors(ISymmetricFinder& finder) {
    HexagonalPacking distr;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(pool, 1000, domain);
    finder.build(pool, storage);

    BruteForceFinder bf;
    bf.build(pool, storage);

    Array<NeighborRecord> treeNeighs;
    Array<NeighborRecord> bfNeighs;
    const Float radius = 0.7_f;

    auto test1 = [&](const Size refIdx) -> Outcome {
        const Size nTree = finder.findAll(refIdx, radius, treeNeighs);
        const Size nBf = bf.findAll(refIdx, radius, bfNeighs);

        if (nTree != nBf) {
            return makeFailed("Invalid number of neighbors:\n{} == {}", nTree, nBf);
        }

        return checkNeighborsEqual(treeNeighs, bfNeighs);
    };
    REQUIRE_SEQUENCE(test1, 0, storage.size());

    auto test2 = [&](const Size refIdx) -> Outcome {
        const Size nTree = finder.findLowerRank(refIdx, radius, treeNeighs);
        const Size nBf = bf.findLowerRank(refIdx, radius, bfNeighs);

        if (nTree != nBf) {
            return makeFailed("Invalid number of neighbors:\n{} == {}", nTree, nBf);
        }

        return checkNeighborsEqual(treeNeighs, bfNeighs);
    };
    REQUIRE_SEQUENCE(test2, 0, storage.size());

    auto test3 = [&](const Size refIdx) -> Outcome {
        // find neighbors in the middle of two points (just to get something else then one of points)
        const Vector point = 0.5_f * (storage[refIdx] + storage[refIdx + 1]);
        const Size nTree = finder.findAll(point, radius, treeNeighs);
        const Size nBf = bf.findAll(point, radius, bfNeighs);

        if (nTree != nBf) {
            return makeFailed("Invalid number of neighbors:\n{} == {}", nTree, nBf);
        }

        return checkNeighborsEqual(treeNeighs, bfNeighs);
    };
    REQUIRE_SEQUENCE(test3, 0, storage.size() - 1);
}

static void checkEmpty(ISymmetricFinder& finder) {
    Array<Vector> storage;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    // build finder on empty array
    REQUIRE_NOTHROW(finder.build(pool, storage));

    // find in empty
    Array<NeighborRecord> treeNeighs;
    const Size nTree = finder.findAll(Vector(0._f), 1.f, treeNeighs);
    REQUIRE(nTree == 0);
}

/// Tests for one particular bug: single particle with very large components of position vector.
/// Used to cause assert in UniformGridFinder, due to absolute values of epsilon in bounding box.
static void checkLargeValues(ISymmetricFinder& finder) {
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    Array<Vector> storage = { Vector(1.e10_f, 2.e10_f, -3.e10_f, 1._f) };
    REQUIRE_NOTHROW(finder.build(pool, storage));

    Array<NeighborRecord> treeNeighs;
    Size nTree = finder.findAll(0, 1.f, treeNeighs);
    REQUIRE(nTree == 1);

    nTree = finder.findLowerRank(0, 1.f, treeNeighs);
    REQUIRE(nTree == 0);
}

/// Tests the ISymmetricFinder::finderLowerRank.
static void checkFindingSmallerH(ISymmetricFinder& finder) {
    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i + 1)); // points on line with increasing H
    }

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    finder.build(pool, storage);
    Array<NeighborRecord> treeNeighs;
    int nAll = finder.findAll(4, 10._f, treeNeighs);
    REQUIRE(nAll == 10); // this should find all particles

    int nSmaller = finder.findLowerRank(4, 10._f, treeNeighs);
    REQUIRE(nSmaller == 4); // this should find indices 0, 1, 2, 3
    bool allMatching = true;
    for (auto& n : treeNeighs) {
        if (n.index > 3) {
            allMatching = false;
        }
    }
    REQUIRE(allMatching);
}

static bool neighborsEqual(Array<Array<NeighborRecord>>& list1, Array<Array<NeighborRecord>>& list2) {
    for (Size i = 0; i < list1.size(); ++i) {
        if (list1[i].size() != list2[i].size()) {
            return false;
        }
        for (Size k = 0; k < list1[i].size(); ++k) {
            if (list1[i][k] != list2[i][k]) {
                return false;
            }
        }
    }
    return true;
}

/// Tests that sequential and parallelized build result in the same thing
static void checkParallelization(ISymmetricFinder& finder) {
    HexagonalPacking distr;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(pool, 100, domain);

    finder.build(SEQUENTIAL, storage);
    Array<Array<NeighborRecord>> sequential;
    for (Size i = 0; i < storage.size(); ++i) {
        Array<NeighborRecord> neighs;
        finder.findAll(storage[i], 2 * storage[i][H], neighs);
        sequential.emplaceBack(std::move(neighs));
    }

    finder.build(pool, storage);
    Array<Array<NeighborRecord>> parallelized;
    for (Size i = 0; i < storage.size(); ++i) {
        Array<NeighborRecord> neighs;
        finder.findAll(storage[i], 2 * storage[i][H], neighs);
        parallelized.emplaceBack(std::move(neighs));
    }

    REQUIRE(sequential.size() == parallelized.size());
    REQUIRE(neighborsEqual(sequential, parallelized));
}

static void testFinder(ISymmetricFinder& finder) {
    checkNeighbors(finder);
    checkEmpty(finder);
    checkLargeValues(finder);
    checkFindingSmallerH(finder);
    checkParallelization(finder);
}


struct NodeData {
    KdNode::Type type = KdNode::Type::LEAF;
    float split = 0.f;
    Size from = 0;
    Size to = 0;

    bool operator!=(const NodeData& other) const {
        return split != other.split || type != other.type || from != other.from || to != other.to;
    }

    friend std::ostream& operator<<(std::ostream& stream, const NodeData& data) {
        stream << int(data.type) << " " << data.split << " " << data.from << " " << data.to;
        return stream;
    }
};

static void checkTreesEqual(KdTree<KdNode>& tree1, KdTree<KdNode>& tree2) {
    // indices of child nodes can be different, but otherwise the split dimensions/positions, bounding boxes,
    // etc. should be the same

    auto getNodeData = [](KdTree<KdNode>& tree) {
        Array<NodeData> data;
        auto functor = [&data](KdNode& node, const KdNode*, const KdNode*) {
            NodeData d;
            d.type = node.type;
            if (node.isLeaf()) {
                LeafNode<KdNode>& leaf = (LeafNode<KdNode>&)node;
                d.from = leaf.from;
                d.to = leaf.to;
            } else {
                InnerNode<KdNode>& inner = (InnerNode<KdNode>&)node;
                d.split = inner.splitPosition;
            }
            data.push(d);
            return true;
        };
        iterateTree<IterateDirection::TOP_DOWN>(tree, SEQUENTIAL, functor);
        return data;
    };

    Array<NodeData> data1 = getNodeData(tree1);
    Array<NodeData> data2 = getNodeData(tree2);
    REQUIRE(data1.size() == data2.size());

    REQUIRE(data1 == data2);
}

TEST_CASE("KdTree", "[finders]") {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(SEQUENTIAL, 1000, domain);

    KdTree<KdNode> finder1;
    finder1.build(*ThreadPool::getGlobalInstance(), storage);
    REQUIRE(finder1.sanityCheck());

    KdTree<KdNode> finder2;
    finder2.build(*Tbb::getGlobalInstance(), storage);
    REQUIRE(finder2.sanityCheck());

    KdTree<KdNode> finder3;
    finder3.build(SEQUENTIAL, storage);
    REQUIRE(finder3.sanityCheck());

    testFinder(finder1);
    testFinder(finder2);
    testFinder(finder3);

    checkTreesEqual(finder1, finder3);
    checkTreesEqual(finder2, finder3);
}


struct TestNode : public KdNode {
    bool visited;

    TestNode(KdNode::Type type)
        : KdNode(type) {
        visited = false;
    }
};

TEST_CASE("KdTree iterateTree bottomUp", "[finders]") {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(SEQUENTIAL, 100000, domain);

    KdTree<TestNode> tree;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    tree.build(pool, storage);

    std::atomic_bool success{ true };
    std::atomic_int visitedCnt{ 0 };
    iterateTree<IterateDirection::BOTTOM_UP>(
        tree, pool, [&](TestNode& node, const TestNode* left, const TestNode* right) {
            if (node.isLeaf()) {
                success = success && (left == nullptr) && (right == nullptr);
            } else {
                success = success && (left != nullptr) && (right != nullptr);
                success = success && left->visited && right->visited;
            }
            node.visited = true;
            visitedCnt++;
            return true;
        });
    REQUIRE(success);
    REQUIRE(Size(visitedCnt) == tree.getNodeCnt());
}

TEST_CASE("KdTree iterateTree topDown", "[finders]") {
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 2._f);
    Array<Vector> storage = distr.generate(SEQUENTIAL, 100000, domain);

    KdTree<TestNode> tree;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    tree.build(pool, storage);

    std::atomic_bool success{ true };
    std::atomic_int visitedCnt{ 0 };

    iterateTree<IterateDirection::TOP_DOWN>(
        tree, pool, [&](TestNode& node, const TestNode* left, const TestNode* right) {
            if (node.isLeaf()) {
                success = success && (left == nullptr) && (right == nullptr);
            } else {
                success = success && (left != nullptr) && (right != nullptr);
                success = success && !left->visited && !right->visited;
            }
            node.visited = true;
            visitedCnt++;
            return true;
        });
    REQUIRE(success);
    REQUIRE(Size(visitedCnt) == tree.getNodeCnt());
}

class KdTreeJobCallbacks : public NullJobCallbacks {
public:
    int checkedCnt = 0;

    virtual void onEnd(const Storage& storage, const Statistics& UNUSED(stats)) override {
        REQUIRE(storage.getParticleCnt() > 10);
        KdTree<KdNode> tree;
        REQUIRE_NOTHROW(tree.build(SEQUENTIAL, storage.getValue<Vector>(QuantityId::POSITION)));
        REQUIRE(tree.sanityCheck());
        checkedCnt++;
    }
};

TEST_CASE("KdTree empty leaf bug", "[finders]") {
    // before 2018-10-23, this test would produce empty leafs in KdTree and fail a sanity check

    CollisionGeometrySettings geometry;
    geometry.set(CollisionGeometrySettingsId::IMPACT_ANGLE, 0._f)
        .set(CollisionGeometrySettingsId::IMPACT_SPEED, 5.e3_f);

    SharedPtr<JobNode> setup = makeNode<CollisionGeometrySetupJob>("collision", geometry);

    BodySettings body;
    body.set(BodySettingsId::BODY_SHAPE_TYPE, DomainEnum::SPHERICAL);
    body.set(BodySettingsId::BODY_RADIUS, 1.e5_f);
    SharedPtr<JobNode> target = makeNode<MonolithicBodyIc>("target", body);
    target->connect(setup, "target");

    body.set(BodySettingsId::BODY_RADIUS, 1.3e4_f);
    SharedPtr<JobNode> impactor = makeNode<MonolithicBodyIc>("impactor", body);
    impactor->connect(setup, "impactor");

    KdTreeJobCallbacks callbacks;
    setup->run(EMPTY_SETTINGS, callbacks);

    // sanity check to make sure the test was actually executed
    REQUIRE(callbacks.checkedCnt == 3);
}

TEST_CASE("UniformGridFinder", "[finders]") {
    UniformGridFinder finder;
    testFinder(finder);
}

TEST_CASE("HashMapFinder", "[finders]") {
    HashMapFinder finder(RunSettings::getDefaults());
    testFinder(finder);
    REQUIRE(finder.good(5));
}

TEST_CASE("HashMapFinder cell size", "[finders]") {
    // tests that bounding box of particles in all cells is below the 2h
    HexagonalPacking distr;
    SphericalDomain domain(Vector(0._f), 3._f);
    Array<Vector> storage = distr.generate(SEQUENTIAL, 100000, domain);
    const Float h0 = storage[0][H];

    HashMapFinder finder(RunSettings::getDefaults());
    finder.build(SEQUENTIAL, storage);
    Outcome result = SUCCESS;
    finder.iterate([&result, h0](const HashMapFinder::Cell& cell, const Box& box) {
        if (box.size() != approx(Vector(2._f * h0))) {
            result = makeFailed("Incorrect cell box size");
        }
        if (!box.contains(cell.box.lower()) || !box.contains(cell.box.upper())) {
            result = makeFailed("Particle box not contained in cell box");
        }
    });
    REQUIRE(result == SUCCESS);
    REQUIRE(finder.good(10));
}

static void testHashMapWithDistr(IDistribution& distr) {
    // SphericalDomain domain(Vector(5._f, -2._f, 9._f), 8._f);
    SphericalDomain domain(Vector(0._f), 8._f);
    Array<Vector> storage = distr.generate(SEQUENTIAL, 100000, domain);
    for (Float h : { 1.e-6_f, 1.e-4_f, 0.01_f, 1._f }) {
        for (Size i = 0; i < storage.size(); ++i) {
            storage[i][H] = h;
        }
        HashMapFinder finder(RunSettings::getDefaults());
        finder.build(SEQUENTIAL, storage);
        REQUIRE(finder.good(8));
    }
}

TEST_CASE("HashMapFinder random good", "[finders]") {
    RandomDistribution distr(1234);
    testHashMapWithDistr(distr);
}

TEST_CASE("HashMapFinder hexa good", "[finders]") {
    HexagonalPacking distr;
    testHashMapWithDistr(distr);
}
