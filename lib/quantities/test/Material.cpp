#include "sph/Materials.h"
#include "catch.hpp"
#include "physics/Eos.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

using namespace Sph;

TEST_CASE("Materials", "[material]") {
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
    REQUIRE(eos1->evaluate(1._f, 1._f)[0] != eos2->evaluate(1._f, 1._f)[0]);
    REQUIRE(eos1->evaluate(1._f, 1._f)[1] != eos2->evaluate(1._f, 1._f)[1]);
}
