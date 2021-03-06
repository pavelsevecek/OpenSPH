#include "quantities/Iterate.h"
#include "catch.hpp"
#include "quantities/Storage.h"

using namespace Sph;

TEST_CASE("Iterate", "[iterate]") {
    Storage storage;
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, makeArray(5._f));
    storage.resize(5);
    storage.insert<Vector>(QuantityId::DENSITY, OrderEnum::FIRST, Vector(1._f));
    storage.insert<SymmetricTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST, SymmetricTensor(3._f));
    storage.insert<TracelessTensor>(QuantityId::ENERGY, OrderEnum::ZERO, TracelessTensor(6._f));

    int cnt = 0;
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [&cnt](const auto& UNUSED(v)) { cnt++; });
    REQUIRE(cnt == 8);

    cnt = 0;
    iterate<VisitorEnum::FIRST_ORDER>(
        storage, [&cnt](const QuantityId, auto&& UNUSED(v), auto&& UNUSED(dv)) { cnt++; });
    REQUIRE(cnt == 2);

    cnt = 0;
    iterate<VisitorEnum::SECOND_ORDER>(storage,
        [&cnt](const QuantityId, auto&& UNUSED(v), auto&& UNUSED(dv), auto&& UNUSED(d2v)) { cnt++; });
    REQUIRE(cnt == 1);

    cnt = 0;
    iterate<VisitorEnum::ZERO_ORDER>(storage, [&cnt](const QuantityId, auto&& UNUSED(dv)) { cnt++; });
    REQUIRE(cnt == 1);

    /// \todo more tests
}

/*TEST_CASE("IterateCustom", "[iterate]") {
    Storage storage;
    storage.insert<Float, OrderEnum::SECOND_ORDER>(QuantityId::POSITIONS, makeArray(5._f));
    storage.resize<VisitorEnum::ALL_BUFFERS>(5);
    storage.insert<Vector, OrderEnum::FIRST_ORDER>(QuantityId::DENSITY, Vector(1._f));
    storage.insert<Tensor, OrderEnum::FIRST_ORDER>(QuantityId::DEVIATORIC_STRESS, Tensor(3._f));

    int i = 0;
    iterateCustom<VisitorEnum::ALL_VALUES>(
        storage, { QuantityId::DEVIATORIC_STRESS, QuantityId::DENSITY }, [&i](auto&& value) {
            if (i++ == 0) {
                REQUIRE((std::is_same<typename std::decay_t<decltype(value)>::Type, Tensor>::value));
                REQUIRE(*reinterpret_cast<Tensor*>(&value[0]) == Tensor(3._f));
            } else {
                REQUIRE((std::is_same<typename std::decay_t<decltype(value)>::Type, Vector>::value));
                REQUIRE(*reinterpret_cast<Vector*>(&value[0]) == Vector(1._f));
            }
        });
}*/
