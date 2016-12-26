#include "benchmark/benchmark_api.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Voxel.h"
#include "sph/initial/Distribution.h"

using namespace Sph;

static void finderRun(benchmark::State& state, Abstract::Finder& finder) {
    HexagonalPacking distribution;
    Array<Vector> r = distribution.generate(10000, SphericalDomain(Vector(0._f), 1._f));
    Array<NeighbourRecord> neighs;
    double distSum = 0.;
    while (state.KeepRunning()) {
        finder.build(r);
        for (int i = 0; i < r.size(); ++i) {
            finder.findNeighbours(i, 2._f * r[i][H], neighs);
            for (NeighbourRecord& n : neighs) {
                distSum += n.distanceSqr;
            }
        }
    }
}

static void kdTreeRun(benchmark::State& state) {
    KdTree tree;
    finderRun(state, tree);
}
BENCHMARK(kdTreeRun);

static void voxelRun(benchmark::State& state) {
    VoxelFinder voxelFiner;
    finderRun(state, voxelFiner);
}
BENCHMARK(voxelRun);


static void bruteForceRun(benchmark::State& state) {
    BruteForceFinder finder;
    finderRun(state, finder);
}
BENCHMARK(bruteForceRun);
