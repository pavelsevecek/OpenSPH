#include "storage/Iterate.h"
#include "catch.hpp"
#include "storage/Storage.h"

using namespace Sph;

TEST_CASE("Iterate", "[iterate]") {
    Storage storage;
    storage.insert<QuantityKey::R, QuantityKey::RHO, QuantityKey::P>();
    int cnt = 0;
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [&cnt](auto&& UNUSED(v)) { cnt++; });
    REQUIRE(cnt == 6);
    iterate<VisitorEnum::FIRST_ORDER>(storage, [&cnt](auto&& UNUSED(v), auto&& UNUSED(dv)) { cnt++; });
    REQUIRE(cnt == 7);
    iterate<VisitorEnum::SECOND_ORDER>(storage,
                                        [&cnt](auto&& UNUSED(v), auto&& UNUSED(dv), auto&& UNUSED(d2v)) {
                                            cnt++;
                                        });
    REQUIRE(cnt == 8);
}
