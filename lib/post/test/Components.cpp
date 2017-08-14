#include "post/Components.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"

using namespace Sph;


TEST_CASE("Components simple", "[components]") {
    Array<Vector> ar{ Vector(0, 0, 0, 1), Vector(5, 0, 0, 1), Vector(0, 4, 0, 1), Vector(0, 3, 0, 1) };
    Array<Size> components;
    RunSettings settings;
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITIONS, OrderEnum::ZERO, std::move(ar));
    Size numComponents = findComponents(storage, settings, ComponentConnectivity::ANY, components);
    REQUIRE(numComponents == 3);
    REQUIRE(components == Array<Size>({ 0, 1, 2, 2 }));
}

TEST_CASE("Component initconds", "[components]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::CUBIC);
    Storage storage;
    InitialConditions conds(storage, RunSettings::getDefaults());
    bodySettings.set<int>(BodySettingsId::PARTICLE_COUNT, 1000);
    conds.addBody(SphericalDomain(Vector(0, 0, 0), 1._f), bodySettings);
    conds.addBody(SphericalDomain(Vector(-6, 4, 0), 1._f), bodySettings);
    conds.addBody(SphericalDomain(Vector(5, 2, 0), 1._f), bodySettings);
    conds.addBody(SphericalDomain(Vector(5, 2.5_f, 0), 1._f), bodySettings);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    Array<Size> components;
    RunSettings settings;
    const Size numComponents = findComponents(storage, settings, ComponentConnectivity::ANY, components);
    REQUIRE(numComponents == 3);
    REQUIRE(components.size() > 0); // sanity check

    auto isMatching = [&](const int i) {
        switch (components[i]) {
        case 0:
            return getLength(r[i] - Vector(0)) <= 1._f;
        case 1:
            return getLength(r[i] - Vector(-6, 4, 0)) <= 1._f;
        case 2:
            return getLength(r[i] - Vector(5, 2, 0)) <= 1._f || getLength(r[i] - Vector(5, 2.5_f, 0)) <= 1._f;
        default:
            return false;
        }
    };
    auto allMatching = [&]() {
        for (Size i = 0; i < components.size(); ++i) {
            if (!isMatching(i)) {
                return false;
            }
        }
        return true;
    };
    REQUIRE(allMatching());
}
