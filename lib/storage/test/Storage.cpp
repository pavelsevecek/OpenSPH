#include "storage/Storage.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Storage", "[storage]") {
    Storage storage = makeStorage<QuantityKey::R, QuantityKey::M>();
    REQUIRE(storage.size() == 2);

    int counter = 0;
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [&counter](auto&& ar) {
        counter++;
        ar.resize(5);
    });
    REQUIRE(counter == 4); // should visit R, dR, d2R and M

    auto tuple = storage.template get<QuantityKey::R, QuantityKey::M>();
    static_assert(tuple.size == 2, "static test failed");
    static_assert(std::is_same<decltype(get<0>(tuple)), ArrayView<Vector>&>::value, "static test failed");
    static_assert(std::is_same<decltype(get<1>(tuple)), ArrayView<Float>&>::value, "static test failed");
    REQUIRE(get<0>(tuple).size() == 5);
    REQUIRE(get<1>(tuple).size() == 5);
}


TEST_CASE("Clone storages", "[storage]") {
    Storage storage     = makeStorage<QuantityKey::R, QuantityKey::RHO, QuantityKey::M>();
    Array<Vector>& rs   = storage.template get<QuantityKey::R>();
    Array<Vector>& vs   = storage.template dt<QuantityKey::R>();
    Array<Vector>& dvs  = storage.template d2t<QuantityKey::R>();
    Array<Float>& ms    = storage.template get<QuantityKey::M>();
    Array<Float>& rhos  = storage.template get<QuantityKey::RHO>();
    Array<Float>& drhos = storage.template dt<QuantityKey::RHO>();

    rs.resize(6);
    vs.resize(5);
    dvs.resize(4);
    ms.resize(3);
    rhos.resize(2);
    drhos.resize(1);

    Storage cloned1 = storage.clone(VisitorEnum::ALL_BUFFERS);
    REQUIRE(cloned1.template get<QuantityKey::R>().size() == 6);
    REQUIRE(cloned1.template dt<QuantityKey::R>().size() == 5);
    REQUIRE(cloned1.template d2t<QuantityKey::R>().size() == 4);
    REQUIRE(cloned1.template get<QuantityKey::M>().size() == 3);
    REQUIRE(cloned1.template get<QuantityKey::RHO>().size() == 2);
    REQUIRE(cloned1.template dt<QuantityKey::RHO>().size() == 1);

    Storage cloned2 = storage.clone(VisitorEnum::HIGHEST_DERIVATIVES);
    REQUIRE(cloned2.template get<QuantityKey::R>().size() == 0);
    REQUIRE(cloned2.template dt<QuantityKey::R>().size() == 0);
    REQUIRE(cloned2.template d2t<QuantityKey::R>().size() == 4);
    REQUIRE(cloned2.template get<QuantityKey::M>().size() == 0);
    REQUIRE(cloned2.template get<QuantityKey::RHO>().size() == 0);
    REQUIRE(cloned2.template dt<QuantityKey::RHO>().size() == 1);

    Storage cloned3 = storage.clone(VisitorEnum::SECOND_ORDER);
    REQUIRE(cloned3.template get<QuantityKey::R>().size() == 0);
    REQUIRE(cloned3.template dt<QuantityKey::R>().size() == 0);
    REQUIRE(cloned3.template d2t<QuantityKey::R>().size() == 4);
    REQUIRE(cloned3.template get<QuantityKey::M>().size() == 0);
    REQUIRE(cloned3.template get<QuantityKey::RHO>().size() == 0);
    REQUIRE(cloned3.template dt<QuantityKey::RHO>().size() == 0);

    cloned3.swap(cloned1, VisitorEnum::ALL_BUFFERS);
    REQUIRE(cloned3.template get<QuantityKey::R>().size() == 6);
    REQUIRE(cloned3.template dt<QuantityKey::R>().size() == 5);
    REQUIRE(cloned3.template d2t<QuantityKey::R>().size() == 4);
    REQUIRE(cloned3.template get<QuantityKey::M>().size() == 3);
    REQUIRE(cloned3.template get<QuantityKey::RHO>().size() == 2);
    REQUIRE(cloned3.template dt<QuantityKey::RHO>().size() == 1);

    REQUIRE(cloned1.template get<QuantityKey::R>().size() == 0);
    REQUIRE(cloned1.template dt<QuantityKey::R>().size() == 0);
    REQUIRE(cloned1.template d2t<QuantityKey::R>().size() == 4);
    REQUIRE(cloned1.template get<QuantityKey::M>().size() == 0);
    REQUIRE(cloned1.template get<QuantityKey::RHO>().size() == 0);
    REQUIRE(cloned1.template dt<QuantityKey::RHO>().size() == 0);

    cloned3.template d2t<QuantityKey::R>().resize(12);
    cloned3.swap(cloned1, VisitorEnum::HIGHEST_DERIVATIVES);
    REQUIRE(cloned3.template get<QuantityKey::R>().size() == 6);
    REQUIRE(cloned3.template dt<QuantityKey::R>().size() == 5);
    REQUIRE(cloned3.template d2t<QuantityKey::R>().size() == 4);
    REQUIRE(cloned3.template get<QuantityKey::M>().size() == 3);
    REQUIRE(cloned3.template get<QuantityKey::RHO>().size() == 2);
    REQUIRE(cloned3.template dt<QuantityKey::RHO>().size() == 0);

    REQUIRE(cloned1.template get<QuantityKey::R>().size() == 0);
    REQUIRE(cloned1.template dt<QuantityKey::R>().size() == 0);
    REQUIRE(cloned1.template d2t<QuantityKey::R>().size() == 12);
    REQUIRE(cloned1.template get<QuantityKey::M>().size() == 0);
    REQUIRE(cloned1.template get<QuantityKey::RHO>().size() == 0);
    REQUIRE(cloned1.template dt<QuantityKey::RHO>().size() == 1);
}
