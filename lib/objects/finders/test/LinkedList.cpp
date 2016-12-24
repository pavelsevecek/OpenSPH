#include "objects/finders/LinkedList.h"
#include "objects/finders/BruteForce.h"
#include "sph/initial/Distribution.h"
#include "catch.hpp"

/// \todo !!

using namespace Sph;

TEST_CASE("Linked List", "[linkedlist]") {
   // HexagonalPacking<> distr;
   // SphericalDomain<> domain(Vector<>(T(0)), T(2));
   // Field<T, Evt::d> storage = distr.generate(50, &domain);

    Array<Vector> storage(0, 10);
    for (int i = 0; i < 10; ++i) {
        storage.push(Vector(i, 0, 0, i)); // points on line with increasing H
    }

    //LinkedList list(storage);
    BruteForceFinder bf;
    bf.build(storage);

    Array<NeighbourRecord> neighs(0, 50);
    //const int cnt = list.findNeighbours(5, 1, neighs);
    const int cntRef = bf.findNeighbours(5, 1, neighs);

    //REQUIRE(cnt == cntRef);

}
