#include "quantities/Storage.h"
#include "catch.hpp"
#include "objects/Exceptions.h"
#include "physics/Eos.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "sph/Materials.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Storage empty", "[storage]") {
    Storage storage;
    REQUIRE(storage.getMaterialCnt() == 0);
    REQUIRE(storage.getQuantityCnt() == 0);
    REQUIRE(storage.getParticleCnt() == 0);
}

TEST_CASE("Storage insert no material", "[storage]") {
    Storage storage;
    REQUIRE_THROWS_AS(
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f), InvalidStorageAccess);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, makeArray(1._f, 2._f));
    REQUIRE(storage.getQuantityCnt() == 1);
    REQUIRE(storage.getQuantity(QuantityId::DENSITY).getOrderEnum() == OrderEnum::FIRST);

    storage.insert<Vector>(QuantityId::DAMAGE, OrderEnum::SECOND, Vector(3._f));
    REQUIRE(storage.getParticleCnt() == 2);
    REQUIRE(storage.getQuantityCnt() == 2);
}

TEST_CASE("Storage insert with material", "[storage]") {
    Storage storage(getMaterial(MaterialEnum::BASALT));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, makeArray(1._f, 2._f));
    REQUIRE(storage.getQuantityCnt() == 2);
    REQUIRE(storage.has(QuantityId::MATERIAL_ID));

    ArrayView<const Size> matId = storage.getValue<Size>(QuantityId::MATERIAL_ID);
    REQUIRE(matId == Array<Size>({ 0, 0 }));
}

TEST_CASE("Storage insert existing by value", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 1._f });
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 1._f);
    REQUIRE(storage.getQuantityCnt() == 1);
    REQUIRE(storage.getQuantity(QuantityId::DENSITY).getOrderEnum() == OrderEnum::FIRST);
    REQUIRE(storage.getValue<Float>(QuantityId::DENSITY)[0] == 1._f);

    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::SECOND, 1._f);
    REQUIRE(storage.getQuantityCnt() == 1);
    REQUIRE(storage.getQuantity(QuantityId::DENSITY).getOrderEnum() == OrderEnum::SECOND);
    REQUIRE(storage.getParticleCnt() == 1);

    REQUIRE_NOTHROW(storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 2._f));
}

TEST_CASE("Storage insert existing by array", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 1._f });
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 4._f });
    REQUIRE(storage.getValue<Float>(QuantityId::DENSITY)[0] == 4._f);

    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::SECOND, Array<Float>{ 5._f });
    REQUIRE(storage.getQuantity(QuantityId::DENSITY).getOrderEnum() == OrderEnum::SECOND);
    REQUIRE(storage.getValue<Float>(QuantityId::DENSITY)[0] == 5._f);

    REQUIRE_THROWS_AS(
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 1._f, 3._f }),
        InvalidStorageAccess);
}

TEST_CASE("Storage resize", "[storage]") {
    Storage storage;
    REQUIRE(storage.getQuantityCnt() == 0);
    REQUIRE(storage.getParticleCnt() == 0);

    Quantity& q1 = storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 5 });
    storage.resize(5);
    REQUIRE(storage.getParticleCnt() == 5);
    Array<Size>& flag = q1.getValue<Size>();
    REQUIRE(flag[0] == 5);
    for (Size i = 1; i < 5; ++i) {
        REQUIRE(flag[i] == 0);
    }
}

TEST_CASE("Storage insert value", "[storage]") {
    Storage storage;
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 5 });
    storage.resize(5);

    Quantity& q2 = storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 3._f);
    REQUIRE(storage.getQuantityCnt() == 2);
    REQUIRE(q2.size() == 5);

    storage.insert<Vector>(QuantityId::MASS, OrderEnum::SECOND, Vector(5._f));
    REQUIRE(storage.getQuantityCnt() == 3);
    REQUIRE(storage.has(QuantityId::DENSITY));
    REQUIRE(storage.has(QuantityId::MASS));
    REQUIRE_FALSE(storage.has(QuantityId::POSITION));
    REQUIRE(storage.has<Float>(QuantityId::DENSITY, OrderEnum::FIRST));
    REQUIRE_FALSE(storage.has<Float>(QuantityId::DENSITY, OrderEnum::SECOND));
    REQUIRE_FALSE(storage.has<Vector>(QuantityId::DENSITY, OrderEnum::FIRST));

    REQUIRE(storage.getValue<Vector>(QuantityId::MASS).size() == 5);
    REQUIRE(storage.getValue<Float>(QuantityId::DENSITY) == Array<Float>({ 3._f, 3._f, 3._f, 3._f, 3._f }));
}

TEST_CASE("Storage resize keep empty", "[storage]") {
    Storage storage;
    Array<Float> values{ 1._f, 2._f, 3._f };
    Quantity& q = storage.insert<Float>(QuantityId::DENSITY, OrderEnum::SECOND, std::move(values));
    q.getValue<Float>().clear();
    q.getDt<Float>().clear();

    storage.resize(6, Storage::ResizeFlag::KEEP_EMPTY_UNCHANGED);
    REQUIRE(q.getValue<Float>().empty());
    REQUIRE(q.getDt<Float>().empty());
    REQUIRE(q.getD2t<Float>().size() == 6);
}

TEST_CASE("Storage resize heterogeneous", "[storage]") {
    Storage storage1(getMaterial(MaterialEnum::BASALT));
    storage1.insert<Float>(QuantityId::DENSITY, OrderEnum::SECOND, Array<Float>{ 1._f, 2._f });
    Storage storage2(getMaterial(MaterialEnum::BASALT));
    storage2.insert<Float>(QuantityId::DENSITY, OrderEnum::SECOND, Array<Float>{ 1._f, 2._f });

    storage1.merge(std::move(storage2));
    REQUIRE(storage1.getMaterialCnt() == 2);

    REQUIRE_ASSERT(storage1.resize(5));
}

TEST_CASE("Storage clone", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0 });
    storage.resize(5);
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, 4._f);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 3._f);


    auto rs = storage.getAll<Float>(QuantityId::POSITION);
    rs[0].resize(6);
    rs[1].resize(5);
    rs[2].resize(4);
    storage.getValue<Float>(QuantityId::MASS).resize(3);
    auto rhos = storage.getAll<Float>(QuantityId::DENSITY);
    rhos[0].resize(2);
    rhos[1].resize(1);

    ArrayView<Float> r, v, dv, m, rho, drho;

    auto updateViews = [&](Storage& st) {
        tie(r, v, dv) = st.getAll<Float>(QuantityId::POSITION);
        tie(rho, drho) = st.getAll<Float>(QuantityId::DENSITY);
        m = st.getValue<Float>(QuantityId::MASS);
    };

    // clone all buffers
    Storage cloned1 = storage.clone(VisitorEnum::ALL_BUFFERS);
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6u, 5u, 4u, 3u, 2u, 1u));

    // only highest derivatives
    Storage cloned2 = storage.clone(VisitorEnum::HIGHEST_DERIVATIVES);
    updateViews(cloned2);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0u, 0u, 4u, 0u, 0u, 1u));

    // only second derivatives
    Storage cloned3 = storage.clone(VisitorEnum::SECOND_ORDER);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0u, 0u, 4u, 0u, 0u, 0u));

    // swap all buffers with 1st storage
    cloned3.swap(cloned1, VisitorEnum::ALL_BUFFERS);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6u, 5u, 4u, 3u, 2u, 1u));
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0u, 0u, 4u, 0u, 0u, 0u));

    cloned3.getAll<Float>(QuantityId::POSITION)[2].resize(12);
    cloned3.swap(cloned1, VisitorEnum::HIGHEST_DERIVATIVES);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6u, 5u, 4u, 3u, 2u, 0u));
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0u, 0u, 12u, 0u, 0u, 1u));
}

TEST_CASE("Storage clone material", "[storage]") {
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1234._f);
    body.set(BodySettingsId::EOS, EosEnum::TAIT);
    AutoPtr<EosMaterial> mat = makeAuto<EosMaterial>(body);
    mat->setRange(QuantityId::AV_ALPHA, Interval(-1._f, 1._f), 0.5_f);
    Storage storage1(std::move(mat));
    storage1.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, { 1._f, 2._f, 3._f });

    body.set(BodySettingsId::DENSITY, 4321._f);
    body.set(BodySettingsId::EOS, EosEnum::MIE_GRUNEISEN);
    mat = makeAuto<EosMaterial>(body);
    mat->setRange(QuantityId::AV_ALPHA, Interval(0._f, 5._f), 2._f);
    Storage storage2(std::move(mat));
    storage2.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, { 4._f, 5._f, 6._f });

    storage1.merge(std::move(storage2));
    // sanity check that we have correct setup
    REQUIRE(storage1.getMaterialCnt() == 2);
    REQUIRE(storage1.getParticleCnt() == 6);
    REQUIRE(storage1.getQuantityCnt() == 2); // positions + matId

    Storage cloned = storage1.clone(VisitorEnum::ALL_BUFFERS);
    REQUIRE(cloned.getMaterialCnt() == 2);
    REQUIRE(cloned.getParticleCnt() == 6);
    REQUIRE(cloned.getQuantityCnt() == 2);

    IMaterial& mat1 = cloned.getMaterial(0);
    REQUIRE(mat1.getParam<Float>(BodySettingsId::DENSITY) == 1234._f);
    REQUIRE(mat1.range(QuantityId::AV_ALPHA) == Interval(-1._f, 1._f));
    REQUIRE(mat1.minimal(QuantityId::AV_ALPHA) == 0.5_f);
    REQUIRE(dynamic_cast<EosMaterial*>(&mat1));

    IMaterial& mat2 = cloned.getMaterial(1);
    REQUIRE(mat2.getParam<Float>(BodySettingsId::DENSITY) == 4321._f);
    REQUIRE(mat2.range(QuantityId::AV_ALPHA) == Interval(0._f, 5._f));
    REQUIRE(mat2.minimal(QuantityId::AV_ALPHA) == 2._f);
    REQUIRE(dynamic_cast<EosMaterial*>(&mat2));

    IMaterial& parentMat1 = storage1.getMaterial(0);
    REQUIRE(
        &dynamic_cast<EosMaterial*>(&mat1)->getEos() == &dynamic_cast<EosMaterial*>(&parentMat1)->getEos());

    mat1.setParam(BodySettingsId::DENSITY, 666._f);
    REQUIRE(parentMat1.getParam<Float>(BodySettingsId::DENSITY) == 666._f);
}

TEST_CASE("Storage merge", "[storage]") {
    Storage storage1;
    storage1.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 0._f, 1._f });

    Storage storage2;
    storage2.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 2._f, 3._f });
    storage1.merge(std::move(storage2));

    REQUIRE(storage1.getQuantityCnt() == 1);
    REQUIRE(storage1.getParticleCnt() == 4);

    ArrayView<const Float> rho = storage1.getValue<Float>(QuantityId::DENSITY);
    REQUIRE(rho == makeArray(0._f, 1._f, 2._f, 3._f));

    // merge into empty
    Storage storage3;
    storage3.merge(std::move(storage1));
    REQUIRE(storage3.getQuantityCnt() == 1);
    REQUIRE(storage3.getParticleCnt() == 4);
}

TEST_CASE("Storage zeroHighestDerivatives", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0 }); // dummy unit
    storage.resize(3);
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, 3._f);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::FIRST, 1._f);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 2._f);

    iterate<VisitorEnum::ALL_BUFFERS>(storage, [](auto& buffer) {
        using Type = typename std::decay_t<decltype(buffer)>::Type;
        buffer.fill(Type(5._f));
    });
    REQUIRE(storage.getAll<Float>(QuantityId::POSITION)[2] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::MASS)[1] == makeArray(5._f, 5._f, 5._f));

    storage.zeroHighestDerivatives();

    REQUIRE(storage.getAll<Float>(QuantityId::POSITION)[2] == makeArray(0._f, 0._f, 0._f));
    REQUIRE(storage.getAll<Float>(QuantityId::POSITION)[1] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::POSITION)[0] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::MASS)[1] == makeArray(0._f, 0._f, 0._f));
    REQUIRE(storage.getAll<Float>(QuantityId::MASS)[0] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::DENSITY)[0] == makeArray(5._f, 5._f, 5._f));
}

TEST_CASE("Storage material", "[storage]") {
    BodySettings settings;
    settings.set(BodySettingsId::ADIABATIC_INDEX, 5._f);
    settings.set(BodySettingsId::SHEAR_MODULUS, 2._f);
    settings.set(BodySettingsId::ELASTICITY_LIMIT, 3._f);
    settings.set<EosEnum>(BodySettingsId::EOS, EosEnum::IDEAL_GAS);

    Storage storage(Factory::getMaterial(settings));
    storage.insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        makeArray(Vector(1._f, 0._f, 0._f), Vector(-2._f, 1._f, 1._f)));

    settings.set(BodySettingsId::ADIABATIC_INDEX, 13._f);
    Storage other(Factory::getMaterial(settings));
    other.insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        makeArray(Vector(-3._f, 4._f, 0._f), Vector(5._f, 1._f, 0._f)));

    storage.merge(std::move(other));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, 1._f);
    auto getPressure = [&](const Size i) {
        EosMaterial& material = dynamic_cast<EosMaterial&>(storage.getMaterialOfParticle(i).material());
        ArrayView<Float> rho, u;
        tie(rho, u) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::ENERGY);
        return material.evaluate(rho[i], u[i])[0];
    };
    REQUIRE(getPressure(0) == 4._f);
    REQUIRE(getPressure(1) == 4._f);
    REQUIRE(getPressure(2) == 12._f);
    REQUIRE(getPressure(3) == 12._f);
}

TEST_CASE("Storage material merge", "[storage]") {
    Storage storage1; // no material
    storage1.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0, 1, 2 });
    Storage storage2(getMaterial(MaterialEnum::BASALT));
    storage2.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 3, 4, 5 });

    REQUIRE_NOTHROW(storage1.merge(std::move(storage2)));
    REQUIRE(storage1.getMaterialCnt() == 2);
    REQUIRE(typeid(storage1.getMaterial(0).material()) == typeid(NullMaterial));
    REQUIRE(typeid(storage1.getMaterial(1).material()) == typeid(SolidMaterial));
    REQUIRE(storage1.isValid());
    REQUIRE(storage2.isValid());

    storage2 = Storage(getMaterial(MaterialEnum::BASALT));
    storage2.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 3, 4, 5 });

    Storage storage3(getMaterial(MaterialEnum::BASALT));
    storage3.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 6, 7, 8 });
    Storage storage4(getMaterial(MaterialEnum::BASALT));
    storage4.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 9, 10 });
    storage2.merge(std::move(storage3));
    storage2.merge(std::move(storage4));

    REQUIRE(storage2.getMaterialCnt() == 3);
    IndexSequence seq1 = storage2.getMaterial(0).sequence();
    REQUIRE(*seq1.begin() == 0);
    REQUIRE(*seq1.end() == 3);
    IndexSequence seq2 = storage2.getMaterial(1).sequence();
    REQUIRE(*seq2.begin() == 3);
    REQUIRE(*seq2.end() == 6);
    IndexSequence seq3 = storage2.getMaterial(2).sequence();
    REQUIRE(*seq3.begin() == 6);
    REQUIRE(*seq3.end() == 8);

    REQUIRE(storage2.getValue<Size>(QuantityId::MATERIAL_ID) == Array<Size>({ 0, 0, 0, 1, 1, 1, 2, 2 }));
}

TEST_CASE("Storage merge to empty", "[storage]") {
    Storage storage(getMaterial(MaterialEnum::BASALT));
    storage.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0, 0 });

    Storage empty;
    empty.merge(std::move(storage));
    REQUIRE(empty.getParticleCnt() == 2);
    REQUIRE(empty.getMaterialCnt() == 1);
}

TEST_CASE("Storage remove", "[storage]") {
    Storage storage1(getMaterial(MaterialEnum::BASALT));
    storage1.getMaterial(0)->setParam(BodySettingsId::PARTICLE_COUNT, 5);
    storage1.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 0, 1, 2 });
    Storage storage2(getMaterial(MaterialEnum::BASALT));
    storage2.getMaterial(0)->setParam(BodySettingsId::PARTICLE_COUNT, 7);
    storage2.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 3, 4, 5 });
    storage1.merge(std::move(storage2));

    storage1.remove(Pair<Size>{ 0, 4 });
    ArrayView<const Size> flags = storage1.getValue<Size>(QuantityId::FLAG);
    REQUIRE(flags == Array<Size>({ 1, 2, 3, 5 }));
    REQUIRE(storage1.getMaterialCnt() == 2);
    REQUIRE(storage1.getMaterial(0)->getParam<int>(BodySettingsId::PARTICLE_COUNT) == 5);
    REQUIRE(storage1.getMaterial(0).sequence() == IndexSequence(0, 2));
    REQUIRE(storage1.getMaterial(1)->getParam<int>(BodySettingsId::PARTICLE_COUNT) == 7);
    REQUIRE(storage1.getMaterial(1).sequence() == IndexSequence(2, 4));

    storage1.remove(Pair<Size>{ 0, 1 });
    flags = storage1.getValue<Size>(QuantityId::FLAG);
    REQUIRE(flags == Array<Size>({ 3, 5 }));
    REQUIRE(storage1.getMaterialCnt() == 1);
    REQUIRE(storage1.getMaterial(0)->getParam<int>(BodySettingsId::PARTICLE_COUNT) == 7);
    REQUIRE(storage1.getMaterial(0).sequence() == IndexSequence(0, 2));
}

TEST_CASE("Storage removeAll", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0 }); // dummy unit
    storage.resize(3);
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, 3._f);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::FIRST, 1._f);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 2._f);

    storage.removeAll();
    REQUIRE(storage.getParticleCnt() == 0);
    REQUIRE(storage.getQuantityCnt() == 0);
    storage.insert<Float>(QuantityId::POSITION, OrderEnum::SECOND, { 3._f, 2._f, 5._f });
    REQUIRE(storage.getParticleCnt() == 3);
    REQUIRE(storage.getQuantityCnt() == 1);
}

TEST_CASE("Storage addDependent", "[storage]") {
    SharedPtr<Storage> storage1 = makeShared<Storage>();
    REQUIRE_ASSERT(storage1->addDependent(storage1));

    storage1->insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0 }); // dummy unit
    SharedPtr<Storage> storage2 = makeShared<Storage>(storage1->clone(VisitorEnum::ALL_BUFFERS));
    storage1->addDependent(storage2);

    REQUIRE(storage1->getParticleCnt() == 1);
    REQUIRE(storage2->getParticleCnt() == 1);

    storage1->resize(5);
    REQUIRE(storage1->getParticleCnt() == 5);
    REQUIRE(storage2->getParticleCnt() == 5);

    storage2->resize(8);
    REQUIRE(storage1->getParticleCnt() == 5);
    REQUIRE(storage2->getParticleCnt() == 8);

    REQUIRE_ASSERT(storage2->addDependent(storage1));
}

TEST_CASE("Storage isValid", "[storage]") {
    Storage storage1(getMaterial(MaterialEnum::BASALT));
    REQUIRE(storage1.isValid());

    storage1.insert<Float>(QuantityId::FLAG, OrderEnum::ZERO, Array<Float>{ 0 });
    REQUIRE(storage1.isValid());

    iterate<VisitorEnum::ALL_BUFFERS>(storage1, [](auto& buffer) { buffer.resize(2); });
    REQUIRE_FALSE(storage1.isValid()); // materials need to be resized as well

    Storage storage2;
    REQUIRE(storage2.isValid());

    storage2.insert<Float>(QuantityId::FLAG, OrderEnum::FIRST, Array<Float>{ 0 });
    REQUIRE(storage2.isValid());

    storage2.getDt<Float>(QuantityId::FLAG).resize(2);
    REQUIRE_FALSE(storage2.isValid()); // derivatives have different size
}

TEST_CASE("Storage persistent indices", "[storage]") {
    Storage storage1;
    storage1.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 0, 1, 2, 3 });
    REQUIRE_FALSE(storage1.has(QuantityId::PERSISTENT_INDEX));

    setPersistentIndices(storage1);
    REQUIRE(storage1.has(QuantityId::PERSISTENT_INDEX));
    ArrayView<const Size> idxs = storage1.getValue<Size>(QuantityId::PERSISTENT_INDEX);
    REQUIRE(idxs == Array<Size>({ 0, 1, 2, 3 }));
    storage1.remove(Array<Size>{ 1 });
    idxs = storage1.getValue<Size>(QuantityId::PERSISTENT_INDEX);
    REQUIRE(idxs == Array<Size>({ 0, 2, 3 }));

    Storage storage2;
    storage2.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 4, 5, 6 });
    setPersistentIndices(storage2);

    storage1.merge(std::move(storage2));
    idxs = storage1.getValue<Size>(QuantityId::PERSISTENT_INDEX);
    REQUIRE(idxs == Array<Size>({ 0, 2, 3, 4, 5, 6 }));
}

TEST_CASE("Storage duplicate", "[storage]") {
    Storage storage1(getMaterial(MaterialEnum::BASALT));
    storage1.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 1, 2, 3 });
    Storage storage2(getMaterial(MaterialEnum::BASALT));
    storage2.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 4, 5, 6, 7 });
    storage1.merge(std::move(storage2));

    REQUIRE(storage1.getMaterialCnt() == 2);
    REQUIRE(storage1.has(QuantityId::MATERIAL_ID));
    Array<Size> createdIdxs =
        storage1.duplicate(Array<Size>{ 0, 2, 3, 5 }, Storage::IndicesFlag::INDICES_SORTED);
    REQUIRE(createdIdxs == Array<Size>({ 9, 10, 3, 4 }));
    REQUIRE(storage1.isValid());

    Array<Size>& flag = storage1.getValue<Size>(QuantityId::FLAG);
    REQUIRE(flag == Array<Size>({ 1, 2, 3, 1, 3, 4, 5, 6, 7, 4, 6 }));

    Array<Size>& ids = storage1.getValue<Size>(QuantityId::MATERIAL_ID);
    REQUIRE(ids == Array<Size>({ 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1 }));

    REQUIRE(storage1.getMaterial(0).sequence() == IndexSequence(0, 5));
    REQUIRE(storage1.getMaterial(1).sequence() == IndexSequence(5, 11));
}
