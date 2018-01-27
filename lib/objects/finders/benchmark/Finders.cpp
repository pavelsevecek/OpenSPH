#include "bench/Session.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/LinkedList.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "sph/initial/Distribution.h"

using namespace Sph;

template <typename TFinder>
void finderRun(Benchmark::Context& context, TFinder& finder, const Size particleCnt) {
    HexagonalPacking distribution;
    Array<Vector> r = distribution.generate(particleCnt, SphericalDomain(Vector(0._f), 1._f));
    Array<NeighbourRecord> neighs;
    double distSum = 0.;
    while (context.running()) {
        finder.build(r);
        for (Size i = 0; i < r.size(); ++i) {
            finder.findAll(i, 2._f * r[i][H], neighs);
            for (NeighbourRecord& n : neighs) {
                distSum += n.distanceSqr;
            }
        }
    }
}

BENCHMARK("Finder run KdTree", "[finders]", Benchmark::Context& context) {
    KdTree tree;
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


/*BENCHMARK("Finder run linkedListRun(benchmark::State& state) {
    LinkedList linkedList;
    finderRun(state, linkedList);
}
BENCHMARK(linkedListRun);


static void finderBuild(benchmark::State& state, INeighbourFinder& finder) {
    HexagonalPacking distribution;
    Array<Vector> r = distribution.generate(10000, SphericalDomain(Vector(0._f), 1._f));
    while (state.KeepRunning()) {
        finder.build(r);
    }
}

static void kdTreeBuild(benchmark::State& state) {
    KdTree tree;
    finderBuild(state, tree);
}
BENCHMARK(kdTreeBuild);

static void voxelBuild(benchmark::State& state) {
    VoxelFinder voxelFiner;
    finderBuild(state, voxelFiner);
}
BENCHMARK(voxelBuild);

static void linkedListBuild(benchmark::State& state) {
    LinkedList linkedList;
    finderBuild(state, linkedList);
}
BENCHMARK(linkedListBuild);
*/
