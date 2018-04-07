#include "post/Analysis.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include "io/Path.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "objects/utility/PerElementWrapper.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Components simple", "[post]") {
    Array<Vector> r{ Vector(0, 0, 0, 1), Vector(5, 0, 0, 1), Vector(0, 4, 0, 1), Vector(0, 3, 0, 1) };
    Array<Size> components;
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(r));
    Size numComponents =
        Post::findComponents(storage, 2._f, Post::ComponentConnectivity::OVERLAP, components);
    REQUIRE(numComponents == 3);
    REQUIRE(components == Array<Size>({ 0, 1, 2, 2 }));
}

TEST_CASE("Component initconds", "[post]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::CUBIC);
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    bodySettings.set<int>(BodySettingsId::PARTICLE_COUNT, 1000);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0, 0, 0), 1._f), bodySettings);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(-6, 4, 0), 1._f), bodySettings);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(5, 2, 0), 1._f), bodySettings);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(5, 2.5_f, 0), 1._f), bodySettings);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<Size> components;
    RunSettings settings;
    const Size numComponents =
        Post::findComponents(storage, 2._f, Post::ComponentConnectivity::OVERLAP, components);
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

TEST_CASE("Component by v_esc", "[post]") {
    Array<Vector> r{ Vector(0, 0, 0, 1), Vector(5, 0, 0, 1), Vector(0, 4, 0, 1), Vector(0, 3, 0, 1) };
    Array<Size> components;
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, std::move(r));
    const Float m0 = 1._f;
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m0);
    Size numComponents =
        Post::findComponents(storage, 2._f, Post::ComponentConnectivity::ESCAPE_VELOCITY, components);
    // all particles still, one component only
    REQUIRE(numComponents == 1);
    REQUIRE(components == Array<Size>({ 0, 0, 0, 0 }));

    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    auto v_esc = [m0](const Float dr) { return sqrt(4._f * m0 * Constants::gravity / dr); };
    v[0] = Vector(0.8_f * v_esc(3._f), 0._f, 0._f);
    // too low velocity, nothing should change
    numComponents =
        Post::findComponents(storage, 2._f, Post::ComponentConnectivity::ESCAPE_VELOCITY, components);
    REQUIRE(numComponents == 1);

    v[0] = Vector(1.2_f * v_esc(3._f), 0._f, 0._f);
    // first and last particle are now separated
    numComponents =
        Post::findComponents(storage, 2._f, Post::ComponentConnectivity::ESCAPE_VELOCITY, components);
    REQUIRE(numComponents == 2);
    REQUIRE(components == Array<Size>({ 0, 1, 1, 1 }));
}


TEST_CASE("CummulativeSfd", "[post]") {
    Array<Vector> r(10);
    for (Size i = 0; i < r.size(); ++i) {
        r[i] = Vector(0._f, 0._f, 0._f, i + 1);
    }
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(r));

    Post::HistogramParams params;
    params.id = Post::HistogramId::RADII;
    params.source = Post::HistogramParams::Source::PARTICLES;
    Array<Post::SfdPoint> points = Post::getCummulativeSfd(storage, params);
    // clang-format off
    Array<Post::SfdPoint> expected{ { 10, 1 }, { 9, 2 }, { 8, 3 }, { 7, 4 }, { 6, 5 },
        { 5, 6 }, { 4, 7 }, { 3, 8 }, { 2, 9 }, { 1, 10 } };
    // clang-format on
    REQUIRE(expected.size() == points.size());

    auto predicate = [](Post::SfdPoint& p1, Post::SfdPoint& p2) { //
        return p1.value == p2.value && p1.count == p2.count;
    };
    REQUIRE(std::equal(points.begin(), points.end(), expected.begin(), predicate));
}

TEST_CASE("ParsePkdgrav", "[post]") {
    // hardcoded path to pkdgrav output
    Path path("/home/pavel/projects/astro/sph/external/sph_0.541_5_45/pkdgrav_run/ss.last.bt");
    if (!FileSystem::pathExists(path)) {
        SKIP_TEST;
    }

    Expected<Storage> storage = Post::parsePkdgravOutput(path);
    REQUIRE(storage);
    REQUIRE(storage->getParticleCnt() > 5000);

    // check that particles are sorted by masses
    ArrayView<Float> m = storage->getValue<Float>(QuantityId::MASS);
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

TEST_CASE("KeplerianElements", "[post]") {
    // test case for Earth
    const Float m = 5.972e24;
    const Float M = 1.989e30;
    const Vector r(0._f, Constants::au, 0._f);
    const Vector v(0._f, 0._f, 29800._f);

    Optional<Post::KeplerianElements> elements = Post::findKeplerEllipse(M + m, m * M / (M + m), r, v);
    REQUIRE(elements);

    REQUIRE(elements->a == approx(Constants::au, 1.e-3f));
    REQUIRE(elements->e == approx(0.0167, 0.1f)); // yup, very uncertain, we just check it's not >1 or whatnot
    REQUIRE(elements->i == approx(PI / 2._f));

    REQUIRE(elements->ascendingNode() == approx(-PI / 2._f));

    // periapsis is too uncertain to actually test anything reasonable
}

TEST_CASE("FindMoons", "[post]") {
    // test case for Earth and Sun -- Earth should be marked as a 'moon' (satellite) of the Sun
    const Float m = 5.972e24;
    const Float M = 1.989e30;
    const Vector r(0._f, Constants::au, 0._f, 0._f);
    const Vector v(0._f, 0._f, 29800._f);

    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, { Vector(0._f), r });
    storage.getDt<Vector>(QuantityId::POSITION) = Array<Vector>{ Vector(0._f), v };
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, { M, m });

    Array<Post::MoonEnum> status = Post::findMoons(storage, 0._f);
    REQUIRE(status.size() == 2);
    REQUIRE(status[0] == Post::MoonEnum::LARGEST_FRAGMENT);
    REQUIRE(status[1] == Post::MoonEnum::MOON);
}
