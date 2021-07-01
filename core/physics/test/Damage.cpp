#include "physics/Damage.h"
#include "catch.hpp"
#include "math/rng/Rng.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "physics/Rheology.h"
#include "post/Analysis.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/ArrayStats.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "timestepping/ISolver.h"
#include "utils/SequenceTest.h"

using namespace Sph;

static void testFractureDistributions(const bool doSampling) {
    ScalarGradyKippModel model;
    BodySettings body;
    body.set(BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS, doSampling);
    Storage storage(Factory::getMaterial(body));
    HexagonalPacking distribution;
    SphericalDomain domain(Vector(0._f), 1._f);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    Array<Vector> r = distribution.generate(pool, 9000, domain);
    const int N = r.size();
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));
    const Float rho0 = body.get<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, rho0 * domain.getVolume() / N);
    MaterialInitialContext context;
    context.rng = makeAuto<RngWrapper<BenzAsphaugRng>>(1234);
    model.setFlaws(storage, storage.getMaterial(0), context);

    // check that all particles have at least one flaw
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    REQUIRE(areAllMatching(n_flaws, [N](const int n) { return n >= 1 && n <= N; }));

    // check that the total number of flaws
    /// \todo how does this depend on N?
    int n_total = 0;
    for (int n : n_flaws) {
        n_total += n;
    }
    const Float m_weibull = body.get<Float>(BodySettingsId::WEIBULL_EXPONENT);
    REQUIRE(n_total >= 8._f * N);
    REQUIRE(n_total <= 11._f * N);

    ArrayStats<Float> mStats(storage.getValue<Float>(QuantityId::M_ZERO));
    ArrayStats<Float> growthStats(storage.getValue<Float>(QuantityId::EXPLICIT_GROWTH));
    ArrayStats<Float> epsStats(storage.getValue<Float>(QuantityId::EPS_MIN));
    REQUIRE(mStats.min() == 1._f);
    REQUIRE(mStats.max() > m_weibull);
    REQUIRE(mStats.average() == approx(m_weibull, 0.05_f));
    REQUIRE(growthStats.min() == growthStats.max());
    REQUIRE(epsStats.min() > 0._f);
    REQUIRE(epsStats.max() == approx(3.e-4_f, 0.2_f));
}

TEST_CASE("Fracture accumulate flaws", "[damage]") {
    testFractureDistributions(false);
}

TEST_CASE("Fracture sample distributios", "[damage]") {
    testFractureDistributions(true);
}

TEST_CASE("Fracture growth", "[damage]") {
    /// \todo some better test, for now just testing that integrate will work without asserts
    ScalarGradyKippModel damage;
    Storage storage;
    InitialConditions conds(RunSettings::getDefaults());
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    conds.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), body);
    ThreadPool& scheduler = *ThreadPool::getGlobalInstance();
    AutoPtr<ISolver> solver = Factory::getSolver(scheduler, RunSettings::getDefaults());
    solver->create(storage, storage.getMaterial(0));

    MaterialInitialContext context;
    context.rng = makeAuto<RngWrapper<BenzAsphaugRng>>(1234);
    MaterialView material = storage.getMaterial(0);
    damage.setFlaws(storage, material, context);
    REQUIRE_NOTHROW(damage.integrate(scheduler, storage, material));

    /// \todo check that if the strain if below eps_min, damage wont increase
}

static void testEquivalence(const Size npart, const int maxdiff) {
    Storage storage;
    RunSettings settings;
    settings.set(RunSettingsId::RUN_RNG, RngEnum::UNIFORM);
    InitialConditions ic(settings);
    BodySettings body;
    body.set(BodySettingsId::PARTICLE_COUNT, int(npart))
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::RANDOM)
        .set(BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS, false);
    ic.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), body);

    body.set(BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS, true);
    ic.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), body);

    ArrayView<Float> m_zero = storage.getValue<Float>(QuantityId::M_ZERO);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);

    // sanity checks (not related to the distributions)
    REQUIRE(storage.getMaterial(0).sequence() == IndexSequence(0, npart));
    REQUIRE(storage.getMaterial(0)->getParam<bool>(BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS) == false);
    REQUIRE(storage.getMaterial(1).sequence() == IndexSequence(npart, 2 * npart));
    REQUIRE(storage.getMaterial(1)->getParam<bool>(BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS) == true);


    // check histograms
    Post::HistogramParams params;
    params.range = Interval(1, 30);
    params.binCnt = Size(params.range.size());
    Array<Float> ns(n_flaws.size());
    for (Size i = 0; i < n_flaws.size(); ++i) {
        ns[i] = Float(n_flaws[i]);
    }

    Array<Post::HistPoint> n1 = Post::getDifferentialHistogram(ns.view().subset(0, npart), params);
    Array<Post::HistPoint> n2 = Post::getDifferentialHistogram(ns.view().subset(npart, npart), params);

    params.binCnt = 100;
    params.range = Interval(1, 50);
    Array<Post::HistPoint> m1 = Post::getDifferentialHistogram(m_zero.subset(0, npart), params);
    Array<Post::HistPoint> m2 = Post::getDifferentialHistogram(m_zero.subset(npart, npart), params);
    REQUIRE(n1.size() == n2.size());
    REQUIRE(m1.size() == m2.size());

    /*FileLogger log(Path("hist_n.txt"));
    for (Size i = 0; i < n1.size(); ++i) {
        log.write(n1[i].value, "  ", n1[i].count, "  ", n2[i].count);
    }

    FileLogger log2(Path("hist_m.txt"));
    for (Size i = 0; i < m1.size(); ++i) {
        log2.write(m1[i].value, "  ", m1[i].count, "  ", m2[i].count);
    }*/

    auto checkBinsN = [&](const Size i) -> Outcome {
        // use absolute, the relative number can easily differ by a lot (it should be compared relative to max
        // value of the distribution, not relative to the other value)
        if (abs(int(n1[i].count) - int(n2[i].count)) > maxdiff) {
            return makeFailed("n_flaws bin different\n", n1[i].count, " == ", n2[i].count);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(checkBinsN, 0, n1.size());

    auto checkBinsM = [&](const Size i) -> Outcome {
        if (abs(int(m1[i].count) - int(m2[i].count)) > maxdiff) {
            return makeFailed("m_zero bin different\n", m1[i].count, " == ", m2[i].count);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(checkBinsM, 0, m1.size());

    // also check individial particles
    std::sort(m_zero.begin(), m_zero.begin() + npart);
    std::sort(m_zero.begin() + npart, m_zero.end());
    std::sort(n_flaws.begin(), n_flaws.begin() + npart);
    std::sort(n_flaws.begin() + npart, n_flaws.end());

    auto checkParticles = [&](const Size i) -> Outcome {
        if (i > npart - 10) {
            // last particles can have extreme value, skip
            return SUCCESS;
        }
        const Size j = i + npart;
        if (m_zero[i] > 1._f && m_zero[i] == m_zero[j]) {
            return makeFailed("m_zero exactly equal, probably using the same code for both");
        }
        if (m_zero[i] != approx(m_zero[j], 0.3_f)) {
            return makeFailed("m_zero different at i=", i, "\n", m_zero[i], " == ", m_zero[j]);
        }
        if (n_flaws[i] != approx(n_flaws[j], 0.35_f)) {
            return makeFailed("n_flaws different at i=", i, "\n", n_flaws[i], " == ", n_flaws[j]);
        }
        return SUCCESS;
    };

    REQUIRE_SEQUENCE(checkParticles, 0, npart);
}

TEST_CASE("Fracture distribution equivalence N = 10 000", "[damage]") {
    testEquivalence(10'000, 500);
}


TEST_CASE("Fracture distribution equivalence N = 100 000", "[damage]") {
    testEquivalence(100'000, 1000);
}


TEST_CASE("Fracture distribution equivalence N = 1 000 000", "[damage]") {
    testEquivalence(1'000'000, 5000);
}
