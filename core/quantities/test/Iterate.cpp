#include "quantities/Iterate.h"
#include "catch.hpp"
#include "objects/wrappers/MultiLambda.h"
#include "quantities/Storage.h"
#include "thread/Pool.h"

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

#ifdef SPH_CPP17

TEST_CASE("Iterate parallel", "[iterate]") {
    Storage storage;
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, makeArray(5._f));
    storage.resize(1);
    storage.insert<Vector>(QuantityId::DENSITY, OrderEnum::FIRST, Vector(1._f));
    storage.insert<TracelessTensor>(QuantityId::ENERGY, OrderEnum::ZERO, TracelessTensor(6._f));

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    iterate<VisitorEnum::ALL_VALUES>(storage,
        pool,
        makeMulti(
            [](QuantityId id, Array<Float>& x) {
                REQUIRE(id == QuantityId::POSITION);
                REQUIRE(x[0] == 5._f);
                x[0] = 2._f;
            },
            [](QuantityId id, Array<Vector>& x) {
                REQUIRE(id == QuantityId::DENSITY);
                REQUIRE(x[0] == Vector(1._f));
                x[0] = Vector(2._f, 1._f, 0._f);
            },
            [](QuantityId id, Array<TracelessTensor>& t) {
                REQUIRE(id == QuantityId::ENERGY);
                REQUIRE(t[0] == TracelessTensor(6._f));
                t[0] = TracelessTensor(3._f);
            },
            [](QuantityId, auto&) { throw; }));

    REQUIRE(storage.getValue<Float>(QuantityId::POSITION)[0] == 2._f);
    REQUIRE(storage.getValue<Vector>(QuantityId::DENSITY)[0] == Vector(2._f, 1._f, 0._f));
    REQUIRE(storage.getValue<TracelessTensor>(QuantityId::ENERGY)[0] == TracelessTensor(3._f));
}

#endif
