#include "bench/Session.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "sph/initial/Distribution.h"
#include "thread/Pool.h"
#include "thread/Tbb.h"

using namespace Sph;

template <typename TFinder>
void finderRun(Benchmark::Context& context, TFinder& finder, const Size particleCnt) {
    HexagonalPacking distribution;
    Tbb& pool = *Tbb::getGlobalInstance();
    Array<Vector> r = distribution.generate(pool, particleCnt, SphericalDomain(Vector(0._f), 1._f));
    Array<NeighborRecord> neighs;
    double distSum = 0.;
    finder.build(pool, r);
    while (context.running()) {
        for (Size i = 0; i < r.size(); ++i) {
            finder.findAll(i, 2._f * r[i][H], neighs);
            for (NeighborRecord& n : neighs) {
                distSum += n.distanceSqr;
            }
        }
    }
}

BENCHMARK("Finder run KdTree", "[finders]", Benchmark::Context& context) {
    KdTree<KdNode> tree;
    finderRun(context, tree, 10000);
}

BENCHMARK("Finder run UniformGrid", "[finders]", Benchmark::Context& context) {
    UniformGridFinder finder;
    finderRun(context, finder, 10000);
}

BENCHMARK("Finder run BruteForce", "[finders]", Benchmark::Context& context) {
    BruteForceFinder bf;
    finderRun(context, bf, 1000);
}

static void finderBuild(Benchmark::Context& context, IBasicFinder& finder, IScheduler& scheduler) {
    HexagonalPacking distribution;
    Array<Vector> r = distribution.generate(scheduler, 1000000, SphericalDomain(Vector(0._f), 1._f));
    while (context.running()) {
        finder.build(scheduler, r);
        Benchmark::clobberMemory();
    }
}

BENCHMARK("Finder build KdTree Sequential", "[finders]", Benchmark::Context& context) {
    KdTree<KdNode> tree;
    finderBuild(context, tree, SEQUENTIAL);
}

BENCHMARK("Finder build KdTree ThreadPool", "[finders]", Benchmark::Context& context) {
    KdTree<KdNode> tree;
    finderBuild(context, tree, *ThreadPool::getGlobalInstance());
}

BENCHMARK("Finder build KdTree Tbb", "[finders]", Benchmark::Context& context) {
    KdTree<KdNode> tree;
    finderBuild(context, tree, *Tbb::getGlobalInstance());
}
