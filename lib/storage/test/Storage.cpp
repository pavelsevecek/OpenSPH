#include "catch.hpp"
#include "storage/Storage.h"

using namespace Sph;

TEST_CASE("Storage", "[storage]") {
    Storage storage = makeStorage<QuantityKey::R, QuantityKey::M>();
    REQUIRE(storage.size() == 2);

    int counter = 0;
    iterate<TemporalEnum::ALL>(storage, [&counter](auto&& ar){
        counter++;
        ar.resize(5);
    });
    REQUIRE(counter == 4);  // should visit R, dR, d2R and M

    auto tuple = storage.template get<QuantityKey::R, QuantityKey::M>();
    static_assert(tuple.size == 2, "static test failed");
    static_assert(std::is_same<decltype(get<0>(tuple)), Array<Vector>&>::value, "static test failed");
    static_assert(std::is_same<decltype(get<1>(tuple)), Array<Float>&>::value, "static test failed");
    REQUIRE(get<0>(tuple).size() == 5);
    REQUIRE(get<1>(tuple).size() == 5);
}
