#include "catch.hpp"
#include "physics/Eos.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "system/Factory.h"

using namespace Sph;

TEST_CASE("IMaterial timestepping params", "[material]") {
    BodySettings settings;
    settings.set(BodySettingsId::DAMAGE_RANGE, Interval(0._f, 10._f));
    settings.set(BodySettingsId::DAMAGE_MIN, 4._f);
    NullMaterial material(settings);
    REQUIRE(material.minimal(QuantityId::POSITION) == 0._f);
    REQUIRE(material.minimal(QuantityId::AV_BALSARA) == 0._f);
    REQUIRE(material.range(QuantityId::DAMAGE) == Interval::unbounded());
    REQUIRE(material.range(QuantityId::DISPLACEMENT) == Interval::unbounded());

    material.setRange(QuantityId::DENSITY, { 1._f, 5._f }, 2._f);
    material.setRange(QuantityId::DAMAGE, BodySettingsId::DAMAGE_RANGE, BodySettingsId::DAMAGE_MIN);
    REQUIRE(material.range(QuantityId::DENSITY) == Interval(1._f, 5._f));
    REQUIRE(material.minimal(QuantityId::DENSITY) == 2._f);
    REQUIRE(material.range(QuantityId::DAMAGE) == Interval(0._f, 10._f));
    REQUIRE(material.minimal(QuantityId::DAMAGE) == 4._f);

    material.setRange(QuantityId::DAMAGE, Interval::unbounded(), 5._f);
    material.setRange(QuantityId::DENSITY, Interval(5._f, 6._f), 0._f);
    REQUIRE(material.range(QuantityId::DAMAGE) == Interval::unbounded());
    REQUIRE(material.minimal(QuantityId::DAMAGE) == 5._f);
    REQUIRE(material.range(QuantityId::DENSITY) == Interval(5._f, 6._f));
    REQUIRE(material.minimal(QuantityId::DENSITY) == 0._f);
}

TEST_CASE("EosMaterials", "[material]") {
    BodySettings settings;
    settings.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS);
    Storage storage(Factory::getMaterial(settings));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 5._f });
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, Array<Float>{ 3._f });

    settings.set(BodySettingsId::EOS, EosEnum::MURNAGHAN);
    Storage storage2(Factory::getMaterial(settings));
    storage2.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, Array<Float>{ 9._f });
    storage2.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, Array<Float>{ 6._f });

    storage.merge(std::move(storage2));
    REQUIRE(storage.getMaterialCnt() == 2);
    EosMaterial* eos1 = dynamic_cast<EosMaterial*>(&storage.getMaterial(0).material());
    REQUIRE(eos1);
    EosMaterial* eos2 = dynamic_cast<EosMaterial*>(&storage.getMaterial(1).material());
    REQUIRE(eos2);
    REQUIRE(eos1->getEos().evaluate(1._f, 1._f)[0] != eos2->getEos().evaluate(1._f, 1._f)[0]);
    REQUIRE(eos1->getEos().evaluate(1._f, 1._f)[1] != eos2->getEos().evaluate(1._f, 1._f)[1]);
}
