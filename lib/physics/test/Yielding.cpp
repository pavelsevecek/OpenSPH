#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "physics/Rheology.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "utils/Utils.h"

using namespace Sph;

TEMPLATE_TEST_CASE("Rheology reduction", "[yielding]", VonMisesRheology, DruckerPragerRheology) {
    TestType rheology;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    Storage storage(getDefaultMaterial());

    Array<Float> energy(10);
    energy.fill(0._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, std::move(energy));
    MaterialView material = storage.getMaterial(0);
    material->setParam(BodySettingsId::MELT_ENERGY, 100._f);
    MaterialInitialContext context;
    rheology.create(storage, storage.getMaterial(0), context);

    const TracelessTensor s0(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    const Float p0 = 10._f;
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, s0);
    storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, p0);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);

    rheology.initialize(pool, storage, material);
    REQUIRE(s[0] == s0); // small stress, elastic material
    REQUIRE(p[0] == p0); // undamaged => unchanged pressure

    Array<Float>& u = storage.getValue<Float>(QuantityId::ENERGY);
    u.fill(120._f);
    rheology.initialize(pool, storage, material);
    REQUIRE(s[0] == TracelessTensor::null()); // u = u_melt, no stress
    REQUIRE(p[0] == p0);                      // u has no effect on p
}

TEMPLATE_TEST_CASE("Rheology repeated", "[yielding]", VonMisesRheology, DruckerPragerRheology) {
    // rheology should not affect already reduced stress tensor
    TestType rheology;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    BodySettings settings;
    settings.set(BodySettingsId::ELASTICITY_LIMIT, 0.5_f);
    settings.set(BodySettingsId::COHESION, 0.5_f);
    Storage storage(Factory::getMaterial(settings));
    Array<Float> energy(1);
    energy.fill(0._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, std::move(energy));
    MaterialInitialContext context;
    rheology.create(storage, storage.getMaterial(0), context);

    const TracelessTensor s0(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    const Float p0 = 1._f;
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::ZERO, s0);
    storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, p0);
    ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    rheology.initialize(pool, storage, storage.getMaterial(0));
    const Float unreduced = ddot(s0, s0);
    const Float reduced1 = ddot(s[0], s[0]);
    REQUIRE(reduced1 > 0);
    REQUIRE(reduced1 < unreduced);

    rheology.initialize(pool, storage, storage.getMaterial(0));
    const Float reduced2 = ddot(s[0], s[0]);
    REQUIRE(reduced1 == approx(reduced2, 1.e-10_f));
}

TEST_CASE("Yielding combinations", "[yielding]") {
    RunSettings settings;
    BodySettings body;
    body.set(BodySettingsId::PARTICLE_COUNT, 10);

    for (YieldingEnum yieldingId :
        { YieldingEnum::NONE, YieldingEnum::NONE, YieldingEnum::VON_MISES, YieldingEnum::DRUCKER_PRAGER }) {
        for (FractureEnum damageId : { FractureEnum::NONE, FractureEnum::SCALAR_GRADY_KIPP }) {

            body.set(BodySettingsId::RHEOLOGY_YIELDING, yieldingId);
            body.set(BodySettingsId::RHEOLOGY_DAMAGE, damageId);

            InitialConditions ic(*ThreadPool::getGlobalInstance(), settings);

            Storage storage;
            /// \todo merging storages with different rheologies (ergo different quantities)
            REQUIRE_NOTHROW(ic.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), body));
        }
    }
}
