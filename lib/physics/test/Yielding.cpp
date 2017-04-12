#include "catch.hpp"
#include "physics/Rheology.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Storage.h"
#include "sph/Material.h"
#include "system/Factory.h"
#include "system/Settings.h"

using namespace Sph;


TEST_CASE("VonMises", "[yielding]") {
    VonMisesRheology vonMises(nullptr);
    Storage storage(getDefaultMaterial());
    Array<Float> energy(10);
    energy.fill(0._f);
    storage.insert<Float>(QuantityIds::ENERGY, OrderEnum::FIRST, std::move(energy));
    MaterialSequence material = storage.getMaterial(0);
    material->setParams(BodySettingsIds::MELT_ENERGY, 100._f);
    material->setParams(BodySettingsIds::ELASTICITY_LIMIT, BodySettings::getDefaults());

    TracelessTensor stress(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    storage.insert<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS, OrderEnum::ZERO, stress);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
    const TracelessTensor unreduced = stress;
    vonMises.initialize(storage, material);
    REQUIRE(unreduced == s[0]); // small stress, elastic material

    Array<Float>& u = storage.getValue<Float>(QuantityIds::ENERGY);
    u.fill(120._f);
    vonMises.initialize(storage, material);
    REQUIRE(s[0] == TracelessTensor::null()); // u = u_melt, no stress
}
