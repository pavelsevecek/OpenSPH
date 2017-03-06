#include "quantities/AbstractMaterial.h"
#include "catch.hpp"
#include "physics/Eos.h"
#include "quantities/Storage.h"

using namespace Sph;


TEST_CASE("EosAccessor", "[material]") {
    BodySettings settings;
    settings.set(BodySettingsIds::EOS, EosEnum::IDEAL_GAS);
    Storage storage(settings);
    storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::DENSITY, Array<Float>{ 5._f });
    storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY, Array<Float>{ 3._f });

    settings.set(BodySettingsIds::EOS, EosEnum::MURNAGHAN);
    Storage storage2(settings);
    storage2.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::DENSITY, Array<Float>{ 9._f });
    storage2.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY, Array<Float>{ 6._f });

    storage.merge(std::move(storage2));
    ArrayView<Material> materials = storage.getMaterials();
    REQUIRE(materials.size() == 2);
    REQUIRE(dynamic_cast<IdealGasEos*>(materials[0].eos.get()));
    REQUIRE(dynamic_cast<MurnaghanEos*>(materials[1].eos.get()));

    EosAccessor eos(storage);
    const Float gamma = settings.get<Float>(BodySettingsIds::ADIABATIC_INDEX);
    REQUIRE(eos.evaluate(0) == IdealGasEos(gamma).evaluate(5._f, 3._f));
    REQUIRE(eos.evaluate(1) == MurnaghanEos(settings).evaluate(9._f, 6._f));
}

TEST_CASE("MaterialAccessor", "[material]") {
    Storage storage(BodySettings::getDefaults());
    storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY, Array<Float>{ 1._f, 1._f });
    MaterialAccessor(storage).setParams(BodySettingsIds::YOUNG_MODULUS, 5._f);

    Storage storage2(BodySettings::getDefaults());
    storage2.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY, Array<Float>{ 2._f, 2._f, 2._f });
    MaterialAccessor(storage2).setParams(BodySettingsIds::YOUNG_MODULUS, 8._f);

    storage.merge(std::move(storage2));
    ArrayView<Material> materials = storage.getMaterials();
    REQUIRE(materials.size() == 2);

    MaterialAccessor material(storage);
    REQUIRE(material.getParam<Float>(BodySettingsIds::YOUNG_MODULUS, 0) == 5._f);
    REQUIRE(material.getParam<Float>(BodySettingsIds::YOUNG_MODULUS, 1) == 5._f);
    REQUIRE(material.getParam<Float>(BodySettingsIds::YOUNG_MODULUS, 2) == 8._f);
    REQUIRE(material.getParam<Float>(BodySettingsIds::YOUNG_MODULUS, 3) == 8._f);
    REQUIRE(material.getParam<Float>(BodySettingsIds::YOUNG_MODULUS, 4) == 8._f);
}
