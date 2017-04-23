#include "quantities/Storage.h"
#include "catch.hpp"
#include "physics/Eos.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Iterate.h"
#include "sph/Material.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Storage insert", "[storage]") {
    Storage storage;
    REQUIRE_ASSERT(storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, makeArray(1._f, 2._f));
    REQUIRE_ASSERT(storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 1._f));
    REQUIRE(storage.getParticleCnt() == 2);
    REQUIRE(storage.getQuantityCnt() == 1);

    storage.insert<Vector>(QuantityId::DAMAGE, OrderEnum::SECOND, Vector(3._f));
    REQUIRE(storage.getParticleCnt() == 2);
    REQUIRE(storage.getQuantityCnt() == 2);
}

TEST_CASE("Storage resize", "[storage]") {
    Storage storage;
    REQUIRE(storage.getQuantityCnt() == 0);
    REQUIRE(storage.getParticleCnt() == 0);

    storage.insert<Size>(QuantityId::MATERIAL_IDX, OrderEnum::ZERO, Array<Size>{ 0 });
    storage.resize(5);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 3._f);
    REQUIRE(storage.getQuantityCnt() == 2);
    REQUIRE(storage.getParticleCnt() == 5);

    storage.insert<Vector>(QuantityId::MASSES, OrderEnum::SECOND, Vector(5._f));
    REQUIRE(storage.getQuantityCnt() == 3);
    REQUIRE(storage.has(QuantityId::DENSITY));
    REQUIRE(storage.has(QuantityId::MASSES));
    REQUIRE(!storage.has(QuantityId::POSITIONS));
    REQUIRE((storage.has<Float, OrderEnum::FIRST>(QuantityId::DENSITY)));
    REQUIRE(!(storage.has<Float, OrderEnum::SECOND>(QuantityId::DENSITY)));
    REQUIRE(!(storage.has<Vector, OrderEnum::FIRST>(QuantityId::DENSITY)));

    REQUIRE(storage.getValue<Vector>(QuantityId::MASSES).size() == 5);
    REQUIRE(storage.getValue<Float>(QuantityId::DENSITY) == Array<Float>({ 3._f, 3._f, 3._f, 3._f, 3._f }));
}

/*TEST_CASE("Storage emplaceWithFunctor", "[storage]") {
    Storage storage;
    Array<Vector> r{ Vector(0._f), Vector(1._f), Vector(2._f), Vector(4._f) };
    Array<Vector> origR = r.clone();
    storage.insert<Vector, OrderEnum::SECOND_ORDER>(QuantityId::POSITIONS, std::move(r));

    int idx = 0;
    storage.emplaceWithFunctor<Vector, OrderEnum::FIRST_ORDER>(
        QuantityId::MASSES, [&idx, &origR](const Vector& v, const int i) {
            REQUIRE(v == origR[idx]);
            REQUIRE(i == idx);
            idx++;

            return Vector(Float(i), 0._f, 0._f);
        });
    REQUIRE(storage.getValue<Vector>(QuantityId::MASSES) == Array<Vector>({ Vector(0._f, 0._f, 0._f),
                                                                 Vector(1._f, 0._f, 0._f),
                                                                 Vector(2._f, 0._f, 0._f),
                                                                 Vector(3._f, 0._f, 0._f) }));
}*/

TEST_CASE("Clone storages", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::MATERIAL_IDX, OrderEnum::ZERO, Array<Float>{ 0 });
    storage.resize(5);
    storage.insert<Float>(QuantityId::POSITIONS, OrderEnum::SECOND, 4._f);
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, 1._f);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 3._f);


    auto rs = storage.getAll<Float>(QuantityId::POSITIONS);
    rs[0].resize(6);
    rs[1].resize(5);
    rs[2].resize(4);
    storage.getValue<Float>(QuantityId::MASSES).resize(3);
    auto rhos = storage.getAll<Float>(QuantityId::DENSITY);
    rhos[0].resize(2);
    rhos[1].resize(1);

    ArrayView<Float> r, v, dv, m, rho, drho;

    auto updateViews = [&](Storage& st) {
        tie(r, v, dv) = st.getAll<Float>(QuantityId::POSITIONS);
        tie(rho, drho) = st.getAll<Float>(QuantityId::DENSITY);
        m = st.getValue<Float>(QuantityId::MASSES);
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

    cloned3.getAll<Float>(QuantityId::POSITIONS)[2].resize(12);
    cloned3.swap(cloned1, VisitorEnum::HIGHEST_DERIVATIVES);
    updateViews(cloned3);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(6u, 5u, 4u, 3u, 2u, 0u));
    updateViews(cloned1);
    REQUIRE(makeArray(r.size(), v.size(), dv.size(), m.size(), rho.size(), drho.size()) ==
            makeArray(0u, 0u, 12u, 0u, 0u, 1u));
}

TEST_CASE("Storage merge", "[storage]") {
    Storage storage1;
    storage1.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 0._f, 1._f });

    Storage storage2;
    storage2.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 2._f, 3._f });
    storage1.merge(std::move(storage2));

    REQUIRE(storage1.getQuantityCnt() == 1);
    REQUIRE(storage1.getParticleCnt() == 4);

    ArrayView<Float> rho = storage1.getValue<Float>(QuantityId::DENSITY);
    REQUIRE(rho == makeArray(0._f, 1._f, 2._f, 3._f));
}

TEST_CASE("Storage init", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::MATERIAL_IDX, OrderEnum::ZERO, Array<Float>{ 0 }); // dummy unit
    storage.resize(3);
    storage.insert<Float>(QuantityId::POSITIONS, OrderEnum::SECOND, 3._f);
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::FIRST, 1._f);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 2._f);

    iterate<VisitorEnum::ALL_BUFFERS>(storage, [](auto&& buffer) {
        using Type = typename std::decay_t<decltype(buffer)>::Type;
        buffer.fill(Type(5._f));
    });
    REQUIRE(storage.getAll<Float>(QuantityId::POSITIONS)[2] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::MASSES)[1] == makeArray(5._f, 5._f, 5._f));

    storage.init();

    REQUIRE(storage.getAll<Float>(QuantityId::POSITIONS)[2] == makeArray(0._f, 0._f, 0._f));
    REQUIRE(storage.getAll<Float>(QuantityId::POSITIONS)[1] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::POSITIONS)[0] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::MASSES)[1] == makeArray(0._f, 0._f, 0._f));
    REQUIRE(storage.getAll<Float>(QuantityId::MASSES)[0] == makeArray(5._f, 5._f, 5._f));
    REQUIRE(storage.getAll<Float>(QuantityId::DENSITY)[0] == makeArray(5._f, 5._f, 5._f));
}

TEST_CASE("Storage material", "[storage]") {
    BodySettings settings;
    settings.set(BodySettingsId::ADIABATIC_INDEX, 5._f);
    settings.set(BodySettingsId::SHEAR_MODULUS, 2._f);
    settings.set(BodySettingsId::ELASTICITY_LIMIT, 3._f);
    settings.set<EosEnum>(BodySettingsId::EOS, EosEnum::IDEAL_GAS);

    Storage storage(Factory::getMaterial(settings));
    storage.insert<Vector>(QuantityId::POSITIONS,
        OrderEnum::SECOND,
        makeArray(Vector(1._f, 0._f, 0._f), Vector(-2._f, 1._f, 1._f)));
    /*Material& mat = storage.getMaterial(0);
    REQUIRE(mat.adiabaticIndex == 5._f);
    REQUIRE(mat.shearModulus == 2._f);
    REQUIRE(mat.elasticityLimit == 3._f);*/

    settings.set<Float>(BodySettingsId::ADIABATIC_INDEX, 13._f);
    Storage other(Factory::getMaterial(settings));
    other.insert<Vector>(QuantityId::POSITIONS,
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

    /// \todo
    /*Array<Material> mats;
    Material m1;
    m1.eos = Factory::getEos(settings);
    mats.push(std::move(m1));

    settings.set<Float>(BodySettingsId::ADIABATIC_INDEX, 25._f);
    Material m2;
    m2.eos = Factory::getEos(settings);
    mats.push(std::move(m2));*/

    /*storage.setMaterial(std::move(mats), [](const Vector& pos, int) {
        if (pos[X] > 0._f) {
            return 0;
        } else {
            return 1;
        }
    });
    REQUIRE(pressure(0) == 12._f);
    REQUIRE(pressure(1) == 24._f);
    REQUIRE(pressure(2) == 24._f);
    REQUIRE(pressure(3) == 12._f);*/
}

TEST_CASE("Storage removeAll", "[storage]") {
    Storage storage;
    storage.insert<Float>(QuantityId::MATERIAL_IDX, OrderEnum::ZERO, Array<Float>{ 0 }); // dummy unit
    storage.resize(3);
    storage.insert<Float>(QuantityId::POSITIONS, OrderEnum::SECOND, 3._f);
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::FIRST, 1._f);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 2._f);

    storage.removeAll();
    REQUIRE(storage.getParticleCnt() == 0);
    REQUIRE(storage.getQuantityCnt() == 0);
    storage.insert<Float>(QuantityId::POSITIONS, OrderEnum::SECOND, { 3._f, 2._f, 5._f });
    REQUIRE(storage.getParticleCnt() == 3);
    REQUIRE(storage.getQuantityCnt() == 1);
}
