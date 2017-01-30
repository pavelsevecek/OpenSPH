#include "sph/initial/Initial.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "quantities/Iterate.h"
#include "quantities/Storage.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "system/Settings.h"

using namespace Sph;

TEST_CASE("Initial conditions", "[initial]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GlobalSettings::getDefaults());
    conds.addBody(domain, bodySettings);

    const Size size = storage->getValue<Vector>(QuantityIds::POSITIONS).size();
    REQUIRE((size >= 80 && size <= 120));
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
