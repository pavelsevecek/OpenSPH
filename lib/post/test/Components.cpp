#include "post/Components.h"
#include "quantities/Storage.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "sph/initial/Initial.h"

using namespace Sph;


TEST_CASE("Components simple", "[components]") {
    Array<Vector> ar{ Vector(0, 0, 0, 1), Vector(5, 0, 0, 1), Vector(0, 4, 0, 1), Vector(0, 3, 0, 1) };
    Array<Size> components;
    Size numComponents = findComponents(ar, GLOBAL_SETTINGS, components);
    REQUIRE(numComponents == 3);
    REQUIRE(components == Array<Size>({ 0, 1, 2, 2 }));
}

TEST_CASE("Component initconds", "[components]") {
    BodySettings bodySettings = BODY_SETTINGS;
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, GLOBAL_SETTINGS);
    bodySettings.set<int>(BodySettingsIds::PARTICLE_COUNT, 1000);
    conds.addBody(SphericalDomain(Vector(0, 0, 0), 1._f), bodySettings);
    conds.addBody(SphericalDomain(Vector(-6, 4, 0), 1._f), bodySettings);
    conds.addBody(SphericalDomain(Vector(5, 2, 0), 1._f), bodySettings);
    conds.addBody(SphericalDomain(Vector(5, 2.5_f, 0), 1._f), bodySettings);

    ArrayView<Vector> r = storage->getValue<Vector>(QuantityIds::POSITIONS);
    Array<Size> components;
    const Size numComponents = findComponents(r, GLOBAL_SETTINGS, components);
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
