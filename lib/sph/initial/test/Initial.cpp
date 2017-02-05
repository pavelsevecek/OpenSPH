#include "sph/initial/Initial.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "quantities/Iterate.h"
#include "quantities/Storage.h"
#include "system/Settings.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("Initial addBody", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    conds.addBody(domain, bodySettings);

    const Size size = storage->getValue<Vector>(QuantityIds::POSITIONS).size();
    REQUIRE(size >= 80);
    REQUIRE(size <= 120);
    iterate<VisitorEnum::ALL_BUFFERS>(*storage, [size](auto&& array) { REQUIRE(array.size() == size); });

    ArrayView<Float> rhos, us, drhos, dus;
    tie(rhos, drhos) = storage->getAll<Float>(QuantityIds::DENSITY);
    tie(us, dus) = storage->getAll<Float>(QuantityIds::ENERGY);
    bool result = areAllMatching(
        rhos, [&](const Float f) { return f == bodySettings.get<Float>(BodySettingsIds::DENSITY); });
    REQUIRE(result);

    result = areAllMatching(drhos, [](const Float f) {
        return f == 0._f; // zero density derivative
    });
    REQUIRE(result);
    result = areAllMatching(
        us, [&](const Float f) { return f == bodySettings.get<Float>(BodySettingsIds::ENERGY); });
    REQUIRE(result);
    result = areAllMatching(dus, [](const Float f) {
        return f == 0._f; // zero energy derivative
    });
    REQUIRE(result);

    ArrayView<Float> ms = storage->getValue<Float>(QuantityIds::MASSES);
    float totalM = 0._f;
    for (Float m : ms) {
        totalM += m;
    }
    REQUIRE(totalM == approx(2700._f * domain.getVolume()));
}

TEST_CASE("Initial velocity", "[initial]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    BodySettings bodySettings;
    bodySettings.set<Float>(BodySettingsIds::DENSITY, 1._f);
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings, Vector(2._f, 1._f, -1._f));
    bodySettings.set<Float>(BodySettingsIds::DENSITY, 2._f);
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings, Vector(0._f, 0._f, 1._f));
    ArrayView<Float> rho = storage->getValue<Float>(QuantityIds::DENSITY);
    ArrayView<Vector> v = storage->getAll<Vector>(QuantityIds::POSITIONS)[1];

    auto test = [&](const Size i) {
        if (rho[i] == 1._f && v[i] != Vector(2._f, 1._f, -1._f)) {
            return makeFailed("Invalid velocity: ", v[i]);
        }
        if (rho[i] == 2._f && v[i] != Vector(0._f, 0._f, 1._f)) {
            return makeFailed("Invalid velocity: ", v[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, v.size());
}

TEST_CASE("Initial rotation", "[initial]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    conds.addBody(SphericalDomain(Vector(0._f), 1._f),
        BodySettings::getDefaults(),
        Vector(0._f),
        Vector(1._f, 3._f, -2._f));
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);

    Vector axis;
    float magnitude;
    tieToTuple(axis, magnitude) = getNormalizedWithLength(Vector(1._f, 3._f, -2._f));

    auto test = [&](const Size i) {
        const Float distFromAxis = getLength(r[i] - axis * dot(r[i], axis));
        if (getLength(v[i]) != approx(distFromAxis * magnitude, 1.e-6_f)) {
            return makeFailed(
                "Invalid angular velocity magnitude: \n", getLength(v[i]), " == ", distFromAxis * magnitude);
        }
        if (dot(v[i], axis) != approx(0._f, 1.e-6_f)) {
            return makeFailed("Invalid angular velocity vector: \n", v[i], " == ", axis);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Initial addHeterogeneousBody single", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 1000);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage1 = std::make_shared<Storage>();
    InitialConditions conds1(storage1, GlobalSettings::getDefaults());

    InitialConditions::Body body1{ domain, bodySettings, Vector(0._f), Vector(0._f) };
    conds1.addHeterogeneousBody(body1, {}); // this should be equal to addBody

    std::shared_ptr<Storage> storage2 = std::make_shared<Storage>();
    InitialConditions conds2(storage2, GlobalSettings::getDefaults());
    conds2.addBody(domain, bodySettings);
    REQUIRE(storage1->getQuantityCnt() == storage2->getQuantityCnt());
    REQUIRE(storage1->getParticleCnt() == storage2->getParticleCnt());
    REQUIRE(storage1->getMaterials().size() == storage2->getMaterials().size());
    iteratePair<VisitorEnum::ALL_BUFFERS>(*storage1, *storage2, [&](auto&& v1, auto&& v2) {
        auto test = [&](const Size i) {
            if (v1[i] != v2[i]) {
                return makeFailed("Different values: ", v1[i], " == ", v2[i]);
            }
            return SUCCESS;
        };
        REQUIRE(v1.size() == v2.size());
        REQUIRE_SEQUENCE(test, 0, v1.size());
    });
}

TEST_CASE("Initial addHeterogeneousBody multiple", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 1000);
    // random to make sure we generate exactly 1000
    bodySettings.set(BodySettingsIds::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM);
    GlobalSettings settings;
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());

    BlockDomain domain(Vector(0._f), Vector(10._f)); // [-5, 5]
    InitialConditions::Body environment{ domain, bodySettings, Vector(0._f), Vector(0._f) };
    const Vector v1(1._f, 2._f, 3._f);
    SphericalDomain domain1(Vector(3._f, 3._f, 2._f), 2._f);
    InitialConditions::Body body1{ domain1, bodySettings, v1, Vector(0._f) };
    const Vector v2(5._f, -1._f, 3._f);
    SphericalDomain domain2(Vector(-2._f, -2._f, -1._f), 2._f);
    InitialConditions::Body body2{ domain2, bodySettings, v2, Vector(0._f) };

    conds.addHeterogeneousBody(environment, Array<InitialConditions::Body>{ body1, body2 });
    REQUIRE(storage->getParticleCnt() == 1000);
    REQUIRE(storage->getMaterials().size() == 3);
    ArrayView<Size> matId = storage->getValue<Size>(QuantityIds::MATERIAL_IDX);
    ArrayView<Size> flag = storage->getValue<Size>(QuantityIds::FLAG);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);
    Size particlesBody1 = 0;
    Size particlesBody2 = 0;
    auto test = [&](const Size i) {
        if (domain1.isInside(r[i])) {
            particlesBody1++;
            return matId[i] == 1 && flag[i] == 0 && v[i] == v1;
        }
        if (domain2.isInside(r[i])) {
            particlesBody2++;
            return matId[i] == 2 && flag[i] == 1 && v[i] == v2;
        }
        return matId[i] == 0 && flag[i] == 2 && v[i] == Vector(0._f);
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
    REQUIRE(particlesBody1 > 30);
    REQUIRE(particlesBody2 > 30);
}
