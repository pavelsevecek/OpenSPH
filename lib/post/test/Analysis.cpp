#include "post/Analysis.h"
#include "catch.hpp"
#include "io/FileSystem.h"
#include "io/Path.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "objects/utility/PerElementWrapper.h"
#include "physics/Constants.h"
#include "physics/Functions.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Components simple", "[post]") {
    Array<Vector> r{ Vector(0, 0, 0, 1), Vector(5, 0, 0, 1), Vector(0, 4, 0, 1), Vector(0, 3, 0, 1) };
    Array<Size> components;
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(r));
    Size numComponents = Post::findComponents(storage, 2._f, Post::ComponentFlag::OVERLAP, components);
    REQUIRE(numComponents == 3);
    REQUIRE(components == Array<Size>({ 0, 1, 2, 2 }));
}

TEST_CASE("Component initconds", "[post]") {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::CUBIC);
    Storage storage;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    InitialConditions conds(pool, RunSettings::getDefaults());
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 1000);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0, 0, 0), 1._f), bodySettings);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(-6, 4, 0), 1._f), bodySettings);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(5, 2, 0), 1._f), bodySettings);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(5, 2.5_f, 0), 1._f), bodySettings);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<Size> components;
    RunSettings settings;
    const Size numComponents = Post::findComponents(storage, 2._f, Post::ComponentFlag::OVERLAP, components);
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
        Post::findComponents(storage, 2._f, Post::ComponentFlag::ESCAPE_VELOCITY, components);
    // all particles still, one component only
    REQUIRE(numComponents == 1);
    REQUIRE(components == Array<Size>({ 0, 0, 0, 0 }));

    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    auto v_esc = [m0](const Float dr) { return sqrt(4._f * m0 * Constants::gravity / dr); };
    v[0] = Vector(0.8_f * v_esc(3._f), 0._f, 0._f);
    // too low velocity, nothing should change
    numComponents = Post::findComponents(storage, 2._f, Post::ComponentFlag::ESCAPE_VELOCITY, components);
    REQUIRE(numComponents == 1);

    v[0] = Vector(1.2_f * v_esc(3._f), 0._f, 0._f);
    // first and last particle are now separated
    numComponents = Post::findComponents(storage, 2._f, Post::ComponentFlag::ESCAPE_VELOCITY, components);
    REQUIRE(numComponents == 2);
    REQUIRE(components == Array<Size>({ 0, 1, 1, 1 }));
}

static Storage getHistogramStorage() {
    Array<Vector> r(10);
    for (Size i = 0; i < r.size(); ++i) {
        r[i] = Vector(0._f, 0._f, 0._f, i + 1);
    }
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(r));
    return storage;
}

TEST_CASE("CumulativeSfd", "[post]") {
    Storage storage = getHistogramStorage();
    Post::HistogramParams params;
    Array<Post::HistPoint> points = Post::getCumulativeHistogram(
        storage, Post::HistogramId::RADII, Post::HistogramSource::PARTICLES, params);

    // clang-format off
    static Array<Post::HistPoint> expected{ { 10, 1 }, { 9, 2 }, { 8, 3 }, { 7, 4 }, { 6, 5 },
        { 5, 6 }, { 4, 7 }, { 3, 8 }, { 2, 9 }, { 1, 10 } };
    // clang-format on
    REQUIRE(expected.size() == points.size());

    REQUIRE(std::equal(points.begin(), points.end(), expected.begin()));
}

TEST_CASE("CumulativeSfd mass cutoff", "[post]") {
    Storage storage = getHistogramStorage();
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, Array<Float>{ 0, 0, 4, 1, 0, 1, 3, 3, 5, 0 });
    Post::HistogramParams params;
    params.massCutoff = 2._f;
    Array<Post::HistPoint> points = Post::getCumulativeHistogram(
        storage, Post::HistogramId::RADII, Post::HistogramSource::PARTICLES, params);

    static Array<Post::HistPoint> expected{ { 9, 1 }, { 8, 2 }, { 7, 3 }, { 3, 4 } };
    REQUIRE(expected.size() == points.size());

    REQUIRE(std::equal(points.begin(), points.end(), expected.begin()));
}

TEST_CASE("CumulativeSfd empty", "[post]") {
    // no values due to too strict cutoffs, make sure to return empty histogram without asserts
    Storage storage = getHistogramStorage();
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
    Post::HistogramParams params;
    params.massCutoff = 10._f;

    Array<Post::HistPoint> points = Post::getCumulativeHistogram(
        storage, Post::HistogramId::RADII, Post::HistogramSource::PARTICLES, params);
    REQUIRE(points.empty());
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

TEST_CASE("Inertia Tensor Sphere", "[post]") {
    const Float r_sphere = 5._f;
    const Float m_tot = 1234._f;
    SphericalDomain sphere(Vector(0._f), r_sphere);
    HexagonalPacking dist;
    Array<Vector> r = dist.generate(SEQUENTIAL, 10000, sphere);
    Array<Float> m(r.size());
    m.fill(m_tot / r.size());

    SymmetricTensor I_sphere = Post::getInertiaTensor(m, r);
    SymmetricTensor I_expected = Rigid::sphereInertia(m_tot, r_sphere);
    REQUIRE(I_sphere.diagonal() == approx(I_expected.diagonal(), 2.e-3_f));
    // the off-diagonal components are generally not zeroes, of course, so just test that they are small
    // compared to the diagonal elements
    REQUIRE(norm(I_sphere.offDiagonal()) < 1.e-3_f * norm(I_sphere.diagonal()));
}

TEST_CASE("Angular Frequency", "[post]") {
    SphericalDomain sphere(Vector(0._f), 4._f);
    HexagonalPacking dist;
    Array<Vector> r = dist.generate(SEQUENTIAL, 10000, sphere);
    Array<Float> m(r.size());
    m.fill(7._f);

    Vector omega(4._f, -7._f, 8._f);
    Array<Vector> v(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = Vector(-1._f, 2._f, 1._f) + cross(omega, r[i]);
    }

    const Vector w = Post::getAngularFrequency(m, r, v, Vector(0._f), Vector(0._f));
    REQUIRE(w == approx(omega, 1.e-3_f));
}
