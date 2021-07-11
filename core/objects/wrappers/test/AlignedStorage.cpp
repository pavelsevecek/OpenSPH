#include "utils/RecordType.h"
#include "objects/wrappers/AlignedStorage.h"
#include "catch.hpp"

using namespace Sph;



TEST_CASE("Emplace", "[shadow]") {
    RecordType::resetStats();
    AlignedStorage<RecordType> as;
    REQUIRE(RecordType::constructedNum == 0);
    as.emplace(5);
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(RecordType::destructedNum == 0);
    REQUIRE(as.get().wasValueConstructed);
    as.destroy();
    REQUIRE(RecordType::destructedNum == 1);
}
