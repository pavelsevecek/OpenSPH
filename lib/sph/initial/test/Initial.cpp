#include "sph/initial/Initial.h"
#include "geometry/Domain.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"
#include "quantities/Storage.h"

using namespace Sph;

TEST_CASE("Initial conditions", "[initial]") {
    Settings<BodySettingsIds> bodySettings(BODY_SETTINGS);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    BlockDomain domain(Vector(0._f), Vector(1._f));
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GLOBAL_SETTINGS);
    conds.addBody(domain, bodySettings);

    const int size = storage->getValue<Vector>(QuantityKey::POSITIONS).size();
    REQUIRE((size >= 80 && size <= 120));
    iterate<VisitorEnum::ALL_BUFFERS>(*storage, [size](auto&& array) { REQUIRE(array.size() == size); });

    ArrayView<Float> rhos, us, drhos, dus;
    tieToArray(rhos, drhos) = storage->getAll<Float>(QuantityKey::DENSITY);
    tieToArray(us, dus) = storage->getAll<Float>(QuantityKey::ENERGY);
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
