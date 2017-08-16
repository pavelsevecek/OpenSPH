#include "post/Analysis.h"
#include "catch.hpp"
#include "io/Path.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/containers/PerElementWrapper.h"
#include "objects/geometry/Domain.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"

using namespace Sph;


TEST_CASE("Components simple", "[post]") {
    Array<Vector> ar{ Vector(0, 0, 0, 1), Vector(5, 0, 0, 1), Vector(0, 4, 0, 1), Vector(0, 3, 0, 1) };
    Array<Size> components;
    RunSettings settings;
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITIONS, OrderEnum::ZERO, std::move(ar));
    Size numComponents =
        Post::findComponents(storage, settings, Post::ComponentConnectivity::ANY, components);
    REQUIRE(numComponents == 3);
    REQUIRE(components == Array<Size>({ 0, 1, 2, 2 }));
}

TEST_CASE("Component initconds", "[post]") {
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
    const Size numComponents =
        Post::findComponents(storage, settings, Post::ComponentConnectivity::ANY, components);
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

TEST_CASE("ParsePkdgrav", "[post]") {
    // hardcoded path to pkdgrav output
    Path path("/home/pavel/projects/astro/sph/external/sph_0.541_5_45/pkdgrav_run/ss.last.bt");

    Expected<Storage> storage = Post::parsePkdgravOutput(path);
    REQUIRE(storage);
    REQUIRE(storage->getParticleCnt() > 5000);

    // check that particles are sorted by masses
    ArrayView<Float> m = storage->getValue<Float>(QuantityId::MASSES);
    Float lastM = LARGE;
    Float sumM = 0._f;
    bool sorted = true;
    for (Size i = 0; i < m.size(); ++i) {
        sumM += m[i];
        if (m[i] > lastM) {
            sorted = false;
        }
        lastM = m[i];
    }
    REQUIRE(sorted);

    // this particular simulation is the impact into 10km target with rho=2700 km/m^3, so the sum of the
    // fragments should be +- as massive as the target
    REQUIRE(sumM == approx(2700 * sphereVolume(5000), 1.e-3_f));

    /*Array<Float>& rho = storage->getValue<Float>(QuantityId::DENSITY);
    REQUIRE(perElement(rho) < 2800._f);
    REQUIRE(perElement(rho) > 2600._f);*/
}
