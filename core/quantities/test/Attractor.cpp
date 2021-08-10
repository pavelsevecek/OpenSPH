#include "quantities/Attractor.h"
#include "catch.hpp"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/CachedGravity.h"
#include "gravity/Moments.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "utils/SequenceTest.h"

using namespace Sph;

template <typename T>
AutoPtr<IGravity> createGravity();

template <>
AutoPtr<IGravity> createGravity<BruteForceGravity>() {
    return makeAuto<BruteForceGravity>(1._f);
}

template <>
AutoPtr<IGravity> createGravity<BarnesHut>() {
    return makeAuto<BarnesHut>(0.4_f, MultipoleOrder::OCTUPOLE, 25, 50, 1._f);
}

template <>
AutoPtr<IGravity> createGravity<CachedGravity>() {
    return makeAuto<CachedGravity>(0.5_f, createGravity<BruteForceGravity>());
}

template <typename T>
const Float gravityEps = EPS;
template <>
const Float gravityEps<BarnesHut> = 2.e-4_f;

TEMPLATE_TEST_CASE("Gravity with attractors", "[gravity]", BruteForceGravity, BarnesHut, CachedGravity) {
    ThreadPool& pool = *ThreadPool::getGlobalInstance();

    RandomDistribution distr(1234);
    SphericalDomain domain1(Vector(0._f), 1.e6_f);
    SphericalDomain domain2(Vector(1.e6_f, 0._f, 0._f), 5.e6_f);
    Array<Vector> points1 = distr.generate(pool, 100, domain1);
    Array<Vector> points2 = distr.generate(pool, 20, domain2);
    const Float m1 = 3.e10_f;
    const Float m2 = 1.5e10_f;

    Storage storage1;
    // combine particles and attractors in storage1
    storage1.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, points1.clone());
    storage1.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m1);
    for (const Vector& p : points2) {
        storage1.addAttractor(Attractor(p, Vector(0._f), p[H], m2));
    }

    Storage storage2;
    // put only particles in storage2
    Array<Vector> allPoints;
    allPoints.pushAll(points1);
    allPoints.pushAll(points2);
    storage2.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(allPoints));
    Array<Float> m(storage2.getParticleCnt());
    for (Size i = 0; i < m.size(); ++i) {
        m[i] = i < points1.size() ? m1 : m2;
    }
    storage2.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(m));

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    ArrayView<Vector> dv1 = storage1.getD2t<Vector>(QuantityId::POSITION);
    AutoPtr<IGravity> gravity = createGravity<TestType>();
    gravity->build(pool, storage1);
    gravity->evalSelfGravity(pool, dv1, stats);
    gravity->evalAttractors(pool, storage1.getAttractors(), dv1);

    ArrayView<Vector> dv2 = storage2.getD2t<Vector>(QuantityId::POSITION);
    gravity->build(pool, storage2);
    gravity->evalSelfGravity(pool, dv2, stats);

    auto test1 = [&](const Size i) -> Outcome {
        if (dv2[i] != approx(dv1[i], gravityEps<TestType>)) {
            return makeFailed("Incorrect acceleration: ", dv2[i], " == ", dv1[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, dv1.size());

    ArrayView<const Attractor> attractors = storage1.getAttractors();
    auto test2 = [&](const Size i) -> Outcome {
        const Vector acc1 = dv2[i + dv1.size()];
        const Vector acc2 = attractors[i].acceleration();
        if (acc1 != approx(acc2, gravityEps<TestType>)) {
            return makeFailed("Incorrect acceleration: ", acc1, " == ", acc2);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, attractors.size());
}
