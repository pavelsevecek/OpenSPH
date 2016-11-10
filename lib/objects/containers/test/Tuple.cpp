#include "objects/containers/Tuple.h"
#include "objects/containers/Array.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Storing references", "[tuple]") {
    int i;
    float f;
    Tuple<int&, float&> t(i, f);
    t = makeTuple(5, 3.14f);
    REQUIRE(i == 5);
    REQUIRE(f == 3.14f);
}

TEST_CASE("Moving tuple", "[tuple]") {
    // using tuple with noncopyable object
    Tuple<Array<int>> t1(Array<int>{5});
    REQUIRE(get<0>(t1)[0] == 5);
}

TEST_CASE("Append", "[tuple]") {
    Tuple<int, float> t1 (5, 1.5f);

    Tuple<int, float, char> t2 = append(t1, 'c');
    REQUIRE(get<0>(t2) == 5);
    REQUIRE(get<1>(t2) == 1.5f);
    REQUIRE(get<2>(t2) == 'c');
}
