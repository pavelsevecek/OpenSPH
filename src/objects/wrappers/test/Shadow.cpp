#include "utils/Utils.h"
#include "objects/wrappers/Shadow.h"
#include "catch.hpp"

using namespace Sph;



TEST_CASE("Emplace", "[shadow]") {
    DummyStruct::constructedNum = 0;
    Shadow<DummyStruct> sd;
    REQUIRE(DummyStruct::constructedNum == 0);
    sd.emplace(5);
    REQUIRE(DummyStruct::constructedNum == 1);
    REQUIRE(sd->value == 5);
    sd.destroy();
    REQUIRE(DummyStruct::constructedNum == 0);
}
