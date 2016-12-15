#include "storage/Storage.h"
#include "catch.hpp"
#include "physics/Eos.h"
#include "system/Factory.h"
#include "system/Settings.h"

using namespace Sph;

TEST_CASE("Storage resize", "[storage]") {
    Storage storage;
    REQUIRE(storage.getQuantityCnt() == 0);
    REQUIRE(storage.getParticleCnt() == 0);
    storage.resize<VisitorEnum::ALL_BUFFERS>(5);
    REQUIRE(storage.getQuantityCnt() == 0);
    REQUIRE(storage.getParticleCnt() == 5);

    storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::RHO, 3._f);
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::M, Vector(5._f));

    REQUIRE(storage.getQuantityCnt() == 2);
    REQUIRE(storage.has(QuantityKey::RHO));
    REQUIRE(storage.has(QuantityKey::M));
    REQUIRE(!storage.has(QuantityKey::R));
    REQUIRE((storage.has<Float, OrderEnum::FIRST_ORDER>(QuantityKey::RHO)));
    REQUIRE(!(storage.has<Float, OrderEnum::SECOND_ORDER>(QuantityKey::RHO)));
    REQUIRE(!(storage.has<Vector, OrderEnum::FIRST_ORDER>(QuantityKey::RHO)));

    REQUIRE(storage.getValue<Vector>(QuantityKey::M).size() == 5);
    REQUIRE(storage.getValue<Float>(QuantityKey::RHO) == Array<Float>({ 3._f, 3._f, 3._f, 3._f, 3._f }));
}

TEST_CASE("Storage emplaceWithFunctor", "[storage]") {
    Storage storage;
    Array<Vector> r{ Vector(0._f), Vector(1._f), Vector(2._f), Vector(4._f) };
    Array<Vector> origR = r.clone();
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::R, std::move(r));

    int idx = 0;
    storage.emplaceWithFunctor<Vector, OrderEnum::FIRST_ORDER>(
        QuantityKey::M, [&idx, &origR](const Vector& v, const int i) {
            REQUIRE(v == origR[idx]);
            REQUIRE(i == idx);
            idx++;

            return Vector(Float(i), 0._f, 0._f);
        });
    REQUIRE(storage.getValue<Vector>(QuantityKey::M) == Array<Vector>({ Vector(0._f, 0._f, 0._f),
                                                            Vector(1._f, 0._f, 0._f),
                                                            Vector(2._f, 0._f, 0._f),
                                                            Vector(3._f, 0._f, 0._f) }));
}

TEST_CASE("Clone storages", "[storage]") {
    Storage storage;
    storage.resize<VisitorEnum::ALL_BUFFERS>(5);
    storage.emplace<Float, OrderEnum::SECOND_ORDER>(QuantityKey::R, 4._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::M, 1._f);
    storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::RHO, 3._f);


    auto rs = storage.getAll<Float>(QuantityKey::R);
    rs[0].resize(6);
    rs[1].resize(5);
    rs[2].resize(4);
    storage.getValue<Float>(QuantityKey::M).resize(3);
    auto rhos = storage.getAll<Float>(QuantityKey::RHO);
    rhos[0].resize(2);
    rhos[1].resize(1);

    ArrayView<Float> r, v, dv, m, rho, drho;

    auto updateViews = [&](Storage& st) {
        tieToArray(r, v, dv) = st.getAll<Float>(QuantityKey::R);
        tieToArray(rho, drho) = st.getAll<Float>(QuantityKey::RHO);
        m = st.getValue<Float>(QuantityKey::M);
    };

    // clone all buffers
    Storage cloned1 = storage.clone(VisitorEnum::ALL_BUFFERS);
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6, 5, 4, 3, 2, 1));

    // only highest derivatives
    Storage cloned2 = storage.clone(VisitorEnum::HIGHEST_DERIVATIVES);
    updateViews(cloned2);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0, 0, 4, 0, 0, 1));

    // only second derivatives
    Storage cloned3 = storage.clone(VisitorEnum::SECOND_ORDER);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0, 0, 4, 0, 0, 0));

    // swap all buffers with 1st storage
    cloned3.swap(cloned1, VisitorEnum::ALL_BUFFERS);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6, 5, 4, 3, 2, 1));
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0, 0, 4, 0, 0, 0));

    cloned3.getAll<Float>(QuantityKey::R)[2].resize(12);
    cloned3.swap(cloned1, VisitorEnum::HIGHEST_DERIVATIVES);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6, 5, 4, 3, 2, 0));
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0, 0, 12, 0, 0, 1));
}

TEST_CASE("Storage merge", "[storage]") {
    Storage storage1;
    storage1.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::RHO, Array<Float>{ 0._f, 1._f });

    Storage storage2;
    storage2.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::RHO, Array<Float>{ 2._f, 3._f });
    storage1.merge(std::move(storage2));

    REQUIRE(storage1.getQuantityCnt() == 1);
    REQUIRE(storage1.getParticleCnt() == 4);

    ArrayView<Float> rho = storage1.getValue<Float>(QuantityKey::RHO);
    REQUIRE(rho == makeArray(0._f, 1._f, 2._f, 3._f));
}

TEST_CASE("Storage init", "[storage]") {
    Storage storage;
    storage.resize<VisitorEnum::ALL_BUFFERS>(3);
    storage.emplace<Float, OrderEnum::SECOND_ORDER>(QuantityKey::R, 3._f);
    storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::M, 1._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::RHO, 2._f);

    iterate<VisitorEnum::ALL_BUFFERS>(storage, [](auto&& buffer) {
        using Type = typename std::decay_t<decltype(buffer)>::Type;
        buffer.fill(Type(5._f));
    });
    REQUIRE(storage.getAll<Float>(QuantityKey::R)[2] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityKey::M)[1] == makeArray(5._f, 5._f, 5._f));

    storage.init();

    REQUIRE(storage.getAll<Float>(QuantityKey::R)[2] == makeArray(0._f, 0._f, 0._f));
    REQUIRE(storage.getAll<Float>(QuantityKey::R)[1] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityKey::R)[0] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityKey::M)[1] == makeArray(0._f, 0._f, 0._f));
    REQUIRE(storage.getAll<Float>(QuantityKey::M)[0] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityKey::RHO)[0] == makeArray(5._f, 5._f, 5._f));
}

TEST_CASE("Storage material", "[storage]") {
    Settings<BodySettingsIds> settings;
    settings.set<Float>(BodySettingsIds::ADIABATIC_INDEX, 5._f);
    settings.set<EosEnum>(BodySettingsIds::EOS, EosEnum::IDEAL_GAS);

    Storage storage(settings);
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::R, makeArray(Vector(1._f, 0._f, 0._f), Vector(-2._f, 1._f, 1._f)));

    settings.set<Float>(BodySettingsIds::ADIABATIC_INDEX, 13._f);
    Storage other(settings);
    other.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::R, makeArray(Vector(-3._f, 4._f, 0._f), Vector(5._f, 1._f, 0._f)));

    storage.merge(std::move(other));
    auto pressure = [&](const int i) { return storage.getMaterial(i).eos->getPressure(1._f, 1._f).get<0>(); };
    REQUIRE(pressure(0) == 4._f);
    REQUIRE(pressure(1) == 4._f);
    REQUIRE(pressure(2) == 12._f);
    REQUIRE(pressure(3) == 12._f);

    Array<Material> mats;
    mats.push(Factory::getEos(settings));
    settings.set<Float>(BodySettingsIds::ADIABATIC_INDEX, 25._f);
    mats.push(Factory::getEos(settings));

    storage.setMaterial(std::move(mats), [](const Vector& pos, int) {
        if (pos[X] > 0._f) {
            return 0;
        } else {
            return 1;
        }
    });
    REQUIRE(pressure(0) == 12._f);
    REQUIRE(pressure(1) == 24._f);
    REQUIRE(pressure(2) == 24._f);
    REQUIRE(pressure(3) == 12._f);
}
