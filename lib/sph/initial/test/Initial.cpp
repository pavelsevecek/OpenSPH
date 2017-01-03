#include "sph/initial/Initial.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "quantities/Storage.h"
#include "system/Logger.h"

using namespace Sph;

TEST_CASE("Initial conditions", "[initial]") {
    Settings<BodySettingsIds> bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GLOBAL_SETTINGS);
    conds.addBody(domain, bodySettings);

    const Size size = storage->getValue<Vector>(QuantityKey::POSITIONS).size();
    REQUIRE((size >= 80 && size <= 120));
    iterate<VisitorEnum::ALL_BUFFERS>(*storage, [size](auto&& array) { REQUIRE(array.size() == size); });

    ArrayView<Float> rhos, us, drhos, dus;
    tie(rhos, drhos) = storage->getAll<Float>(QuantityKey::DENSITY);
    tie(us, dus) = storage->getAll<Float>(QuantityKey::ENERGY);
    bool result = areAllMatching(rhos, [](const Float f) {
        return f == 2700._f; // density of 2700km/m^3
    });
    REQUIRE(result);

    result = areAllMatching(drhos, [](const Float f) {
        return f == 0._f; // zero density derivative
    });
    REQUIRE(result);
    result = areAllMatching(us, [](const Float f) {
        return f == 0._f; // zero internal energy
    });
    REQUIRE(result);
    result = areAllMatching(dus, [](const Float f) {
        return f == 0._f; // zero energy derivative
    });
    REQUIRE(result);

    ArrayView<Float> ms = storage->getValue<Float>(QuantityKey::MASSES);
    float totalM = 0._f;
    for (Float m : ms) {
        totalM += m;
    }
    REQUIRE(Math::almostEqual(totalM, 2700._f * domain.getVolume()));
}

TEST_CASE("Initial velocity", "[initial]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GLOBAL_SETTINGS);
    BodySettings bodySettings = BODY_SETTINGS;
    bodySettings.set<Float>(BodySettingsIds::DENSITY, 1._f);
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings, Vector(2._f, 1._f, -1._f));
    bodySettings.set<Float>(BodySettingsIds::DENSITY, 2._f);
    conds.addBody(SphericalDomain(Vector(0._f), 1._f), bodySettings, Vector(0._f, 0._f, 1._f));
    ArrayView<Float> rho = storage->getValue<Float>(QuantityKey::DENSITY);
    ArrayView<Vector> v = storage->getAll<Vector>(QuantityKey::POSITIONS)[1];

    StdOutLogger logger;
    bool allMatching = true;
    for (Size i = 0; i < v.size(); ++i) {
        if (rho[i] == 1._f && v[i] != Vector(2._f, 1._f, -1._f)) {
            allMatching = false;
            logger.writeList("Invalid velocity: ", v[i]);
            break;
        }
        if (rho[i] == 2._f && v[i] != Vector(0._f, 0._f, 1._f)) {
            allMatching = false;
            logger.writeList("Invalid velocity: ", v[i]);
            break;
        }
    }
    REQUIRE(allMatching);
}

TEST_CASE("Initial rotation", "[initial]") {
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GLOBAL_SETTINGS);
    conds.addBody(
        SphericalDomain(Vector(0._f), 1._f), BODY_SETTINGS, Vector(0._f), Vector(1._f, 3._f, -2._f));
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityKey::POSITIONS);

    Vector axis;
    float magnitude;
    tieToTuple(axis, magnitude) = getNormalizedWithLength(Vector(1._f, 3._f, -2._f));

    bool allMatching = true;
    StdOutLogger logger;
    for (Size i = 0; i < r.size(); ++i) {
        const Float distFromAxis = getLength(r[i] - axis * dot(r[i], axis));
        if (!Math::almostEqual(getLength(v[i]), distFromAxis * magnitude)) {
            allMatching = false;
            logger.writeList(
                "Invalid angular velocity magnitude: ", getLength(v[i]), " / ", distFromAxis * magnitude);
            break;
        }
        if (!Math::almostEqual(dot(v[i], axis), 0._f)) {
            allMatching = false;
            logger.writeList("Invalid angular velocity vector: ", v[i], " / ", axis);
            break;
        }
    }
    REQUIRE(allMatching);
}
