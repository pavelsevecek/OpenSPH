#include "sph/equations/Accumulated.h"
#include "catch.hpp"
#include "objects/utility/PerElementWrapper.h"
#include "thread/Pool.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Accumulated sum simple", "[accumulated]") {
    Accumulated ac1;
    REQUIRE_ASSERT(ac1.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO));
    REQUIRE(ac1.getBufferCnt() == 0);
    ac1.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, BufferSource::SHARED);
    REQUIRE(ac1.getBufferCnt() == 1);
    // subsequent calls dont do anything
    ac1.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, BufferSource::SHARED);
    REQUIRE(ac1.getBufferCnt() == 1);

    ac1.initialize(5);
    ArrayView<Size> buffer1 = ac1.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO);
    REQUIRE(buffer1.size() == 5);
    REQUIRE_NOTHROW(ac1.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO));
    REQUIRE_ASSERT(ac1.getBuffer<Float>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO));
    REQUIRE_ASSERT(ac1.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::FIRST));
    REQUIRE(ac1.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO).size() == 5);
    REQUIRE(ac1.getBufferCnt() == 1);

    Accumulated ac2;
    ac2.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, BufferSource::SHARED);
    ac2.initialize(5);
    ArrayView<Size> buffer2 = ac2.getBuffer<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO);
    REQUIRE(ac2.getBufferCnt() == 1);
    for (Size i = 0; i < 5; ++i) {
        buffer1[i] = i;
        buffer2[i] = 5 - i;
    }

    ac1.sum(Array<Accumulated*>{ &ac2 });
    REQUIRE(perElement(buffer1) == 5);
}

template <typename TValue>
ArrayView<TValue> getInserted(Accumulated& ac, const QuantityId id, const Size size) {
    ac.insert<TValue>(id, OrderEnum::ZERO, BufferSource::SHARED);
    ac.initialize(size); // multiple initialization do not matter, it's only a bit inefficient
    return ac.getBuffer<TValue>(id, OrderEnum::ZERO);
}

static Accumulated getAccumulated() {
    Accumulated ac;
    ArrayView<Size> buffer1 = getInserted<Size>(ac, QuantityId::NEIGHBOUR_CNT, 5);
    ArrayView<Float> buffer2 = getInserted<Float>(ac, QuantityId::DENSITY, 5);
    ArrayView<Vector> buffer3 = getInserted<Vector>(ac, QuantityId::ENERGY, 5);
    ArrayView<SymmetricTensor> buffer4 = getInserted<SymmetricTensor>(ac, QuantityId::POSITION, 5);
    for (Size i = 0; i < 5; ++i) {
        buffer1[i] = 5;
        buffer2[i] = 3._f;
        buffer3[i] = Vector(2._f);
        buffer4[i] = SymmetricTensor(1._f);
    }
    return ac;
}

static Storage getStorage() {
    Storage storage;
    storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, Array<Size>{ 1 });
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 0._f);
    storage.insert<Vector>(QuantityId::ENERGY, OrderEnum::ZERO, Vector(0._f));
    storage.insert<SymmetricTensor>(QuantityId::POSITION, OrderEnum::ZERO, SymmetricTensor::null());
    return storage;
}

TEST_CASE("Accumulated sum parallelized", "[accumulated]") {
    Accumulated ac1 = getAccumulated();
    Accumulated ac2 = getAccumulated();
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    ac1.sum(pool, Array<Accumulated*>{ &ac2 });
    Storage storage = getStorage();
    ac1.store(storage);

    REQUIRE(storage.getQuantityCnt() == 4);
    REQUIRE(storage.getParticleCnt() == 5);
    ArrayView<Size> buffer1 = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    REQUIRE(buffer1.size() == 5);
    REQUIRE(perElement(buffer1) == 10);
    ArrayView<Float> buffer2 = storage.getValue<Float>(QuantityId::DENSITY);
    REQUIRE(buffer2.size() == 5);
    REQUIRE(perElement(buffer2) == 6._f);
    ArrayView<Vector> buffer3 = storage.getValue<Vector>(QuantityId::ENERGY);
    REQUIRE(buffer3.size() == 5);
    REQUIRE(perElement(buffer3) == Vector(4._f));
    ArrayView<SymmetricTensor> buffer4 = storage.getValue<SymmetricTensor>(QuantityId::POSITION);
    REQUIRE(buffer4.size() == 5);
    REQUIRE(perElement(buffer4) == SymmetricTensor(2._f));
}

TEST_CASE("Accumulated store", "[accumulated]") {
    Accumulated ac;
    ArrayView<Size> buffer1 = getInserted<Size>(ac, QuantityId::NEIGHBOUR_CNT, 5);
    for (Size i = 0; i < 5; ++i) {
        buffer1[i] = i;
    }
    Storage storage = getStorage();
    ac.store(storage);
    ArrayView<Size> buffer2 = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    REQUIRE(buffer2.size() == 5);
    for (Size i = 0; i < 5; ++i) {
        REQUIRE(buffer2[i] == i);
    }
}

TEST_CASE("Accumulate store second derivative", "[accumulated]") {
    Accumulated ac;
    ac.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
    ac.initialize(1);
    ArrayView<Vector> dv = ac.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    dv[0] = Vector(5._f);

    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, makeArray(Vector(0._f)));
    REQUIRE_ASSERT(ac.store(storage));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, Vector(0._f));
    REQUIRE_NOTHROW(ac.store(storage));
    dv = storage.getD2t<Vector>(QuantityId::POSITION);
    REQUIRE(dv[0] == Vector(5._f));
}

TEST_CASE("Accumulated insert two orders", "[accumulated]") {
    Accumulated ac;
    ac.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
    REQUIRE_ASSERT(ac.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, BufferSource::SHARED));
    REQUIRE_ASSERT(ac.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::FIRST));
}
