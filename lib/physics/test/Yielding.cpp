#include "catch.hpp"
#include "physics/Rheology.h"
#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "tests/Approx.h"

using namespace Sph;


TEST_CASE("VonMises reduction", "[yielding]") {
    VonMisesRheology vonMises;
    Storage storage(getDefaultMaterial());

    Array<Float> energy(10);
    energy.fill(0._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, std::move(energy));
    MaterialView material = storage.getMaterial(0);
    material->setParam(BodySettingsId::MELT_ENERGY, 100._f);
    MaterialInitialContext context;
    vonMises.create(storage, storage.getMaterial(0), context);

    TracelessTensor stress(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, stress);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    const TracelessTensor unreduced = stress;
    vonMises.initialize(storage, material);
    REQUIRE(unreduced == s[0]); // small stress, elastic material

    Array<Float>& u = storage.getValue<Float>(QuantityId::ENERGY);
    u.fill(120._f);
    vonMises.initialize(storage, material);
    REQUIRE(s[0] == TracelessTensor::null()); // u = u_melt, no stress
}

TEST_CASE("VonMises repeated", "[yielding]") {
    // von Mises should not affect already reduced stress tensor
    VonMisesRheology vonMises;
    BodySettings settings;
    settings.set<Float>(BodySettingsId::ELASTICITY_LIMIT, 0.5_f);
    Storage storage(Factory::getMaterial(settings));
    Array<Float> energy(1);
    energy.fill(0._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, std::move(energy));
    MaterialInitialContext context;
    vonMises.create(storage, storage.getMaterial(0), context);

    TracelessTensor stress(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, stress);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    vonMises.initialize(storage, storage.getMaterial(0));
    const Float unreduced = ddot(stress, stress);
    const Float reduced1 = ddot(s[0], s[0]);
    REQUIRE(reduced1 > 0);
    REQUIRE(reduced1 < unreduced);

    vonMises.initialize(storage, storage.getMaterial(0));
    const Float reduced2 = ddot(s[0], s[0]);
    REQUIRE(reduced1 == approx(reduced2));
}
