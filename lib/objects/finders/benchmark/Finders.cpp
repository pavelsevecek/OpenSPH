#include "bench/Session.h"
#include "geometry/Domain.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/LinkedList.h"
#include "objects/finders/Voxel.h"
#include "sph/initial/Distribution.h"

using namespace Sph;

template <typename TFinder>
void finderRun(Benchmark::Context& context, TFinder& finder) {
    HexagonalPacking distribution;
    Array<Vector> r = distribution.generate(1e4, SphericalDomain(Vector(0._f), 1._f));
    Array<NeighbourRecord> neighs;
    double distSum = 0.;
    while (context.running()) {
        finder.build(r);
        for (Size i = 0; i < r.size(); ++i) {
            finder.findNeighbours(i, 2._f * r[i][H], neighs);
            for (NeighbourRecord& n : neighs) {
                distSum += n.distanceSqr;
            }
        }
    }
}

BENCHMARK("Finder run KdTree", "[finders]", Benchmark::Context& context) {
    KdTree tree;
    finderRun(context, tree);
}

BENCHMARK("Finder run Boxel", "[finders]", Benchmark::Context& context) {
    VoxelFinder voxelFiner;
    finderRun(context, voxelFiner);
}

BENCHMARK("Finder run BruteForce", "[finders]", Benchmark::Context& context) {
    BruteForceFinder bf;
    finderRun(context, bf);
}


/*BENCHMARK("Finder run linkedListRun(benchmark::State& state) {
    LinkedList linkedList;
    finderRun(state, linkedList);
}
BENCHMARK(linkedListRun);


static void finderBuild(benchmark::State& state, Abstract::Finder& finder) {
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
