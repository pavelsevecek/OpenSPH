#include "storage/Iterate.h"
#include "catch.hpp"
#include "storage/Storage.h"

using namespace Sph;

TEST_CASE("Iterate", "[iterate]") {
    Storage storage;
    storage.resize<VisitorEnum::ALL_BUFFERS>(5);
    storage.emplace<Float, OrderEnum::SECOND_ORDER>(QuantityKey::R, 5._f);
    storage.emplace<Vector, OrderEnum::FIRST_ORDER>(QuantityKey::RHO, Vector(1._f));
    storage.emplace<Tensor, OrderEnum::FIRST_ORDER>(QuantityKey::S, Tensor(3._f));
    storage.emplace<TracelessTensor, OrderEnum::ZERO_ORDER>(QuantityKey::BETA, TracelessTensor(6._f));

    int cnt = 0;
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [&cnt](auto&& UNUSED(v)) { cnt++; });
    REQUIRE(cnt == 8);

    cnt = 0;
    iterate<VisitorEnum::FIRST_ORDER>(storage, [&cnt](auto&& UNUSED(v), auto&& UNUSED(dv)) { cnt++; });
    REQUIRE(cnt == 2);

    cnt = 0;
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [&cnt](auto&& UNUSED(v), auto&& UNUSED(dv), auto&& UNUSED(d2v)) { cnt++; });
    REQUIRE(cnt == 1);

    cnt = 0;
    iterate<VisitorEnum::ZERO_ORDER>(storage, [&cnt](auto&& UNUSED(dv)) { cnt++; });
    REQUIRE(cnt == 1);
}
