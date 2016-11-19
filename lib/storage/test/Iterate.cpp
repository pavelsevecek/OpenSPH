#include "catch.hpp"
#include "storage/Iterate.h"
#include "storage/Storage.h"

using namespace Sph;

TEST_CASE("Iterate", "[iterate]") {
    Storage storage;
    storage.insert<QuantityKey::R, QuantityKey::RHO, QuantityKey::P>();
    int cnt = 0;
    iterate<TemporalEnum::ALL>(storage, [&cnt](auto&& UNUSED(v)){
        cnt++;
    });
    REQUIRE(cnt == 6);
    iterate<TemporalEnum::FIRST_ORDER>(storage, [&cnt](auto&& UNUSED(v), auto&& UNUSED(dv)){
        cnt++;
    });
    REQUIRE(cnt == 7);
    iterate<TemporalEnum::SECOND_ORDER>(storage, [&cnt](auto&& UNUSED(v), auto&& UNUSED(dv)){
        cnt++;
    });
    REQUIRE(cnt == 8);
}
