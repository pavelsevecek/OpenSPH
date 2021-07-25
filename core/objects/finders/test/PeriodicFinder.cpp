#include "objects/finders/PeriodicFinder.h"
#include "catch.hpp"
#include "math/MathUtils.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "sph/initial/Distribution.h"

using namespace Sph;


TEST_CASE("PeriodicFinder", "[finders]") {
    Box box(Vector(0._f), Vector(2._f, 1._f, 1._f));
    PeriodicFinder finder(makeAuto<UniformGridFinder>(), box, SEQUENTIAL.getGlobalInstance());

    BlockDomain domain(box.center(), box.size());
    HexagonalPacking dist;
    Array<Vector> r = dist.generate(SEQUENTIAL, 100000, domain);

    finder.build(SEQUENTIAL, r);

    Array<NeighborRecord> neighs;
    const Float radius = 0.1_f;
    finder.findAll(Vector(0._f, 0.5_f, 0.5_f), radius, neighs);

    REQUIRE_FALSE(neighs.empty());
    auto iter = std::find_if(neighs.begin(), neighs.end(), [&](NeighborRecord& n) { //
        return r[n.index][X] > 1._f;
    });
    REQUIRE(iter != neighs.end());
    // even though the particle is far, the returned distance respects the periodicity (no longer simple
    // distance metric)
    REQUIRE(iter->distanceSqr < sqr(radius));
}
