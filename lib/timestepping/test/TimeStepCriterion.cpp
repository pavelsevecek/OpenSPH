#include "timestepping/TimeStepCriterion.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "thread/Pool.h"
#include "utils/Utils.h"

using namespace Sph;

const Size MINIMAL_PARTICLE_IDX = 53; // just something non-trivial

static Storage getStorage() {
    Storage storage(getMaterial(MaterialEnum::BASALT));
    HexagonalPacking distribution;
    storage.insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        distribution.generate(SEQUENTIAL, 100, BlockDomain(Vector(0._f), Vector(100._f))));
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, 0._f);
    IMaterial& material = storage.getMaterial(0);
    material.setRange(QuantityId::ENERGY, Interval::unbounded(), EPS);

    // setup sound speed to 5 for all particles
    const Float cs = 5._f;
    storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, cs);


    // some non-trivial values of energy and derivative
    ArrayView<Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityId::ENERGY);
    for (Size i = 0; i < u.size(); ++i) {
        u[i] = 12._f + Sph::abs(MINIMAL_PARTICLE_IDX - i); // minimal value is 12 for particle 53
        du[i] = 4._f;                                      // du/dt = 4
    }

    return storage;
}

TEST_CASE("Courant Criterion", "[timestepping]") {
    CourantCriterion cfl(RunSettings::getDefaults());
    const Float courantNumber =
        RunSettings::getDefaults().get<Float>(RunSettingsId::TIMESTEPPING_COURANT_NUMBER);

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    Storage storage = getStorage();

    Statistics stats;
    TimeStep step = cfl.compute(pool, storage, INFTY, stats);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    const Float h = r[0][H]; // all hs are the same
    const Float expected = courantNumber * h / cs[0];
    REQUIRE(expected == approx(step.value));
    REQUIRE(step.id == CriterionId::CFL_CONDITION);

    // get timestep limited by max value
    step = cfl.compute(pool, storage, 1.e-3_f, stats);
    REQUIRE(step.value == 1.e-3_f);
    REQUIRE(step.id == CriterionId::MAXIMAL_VALUE);
}

TEST_CASE("Derivative Criterion minimum", "[timestepping]") {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    DerivativeCriterion criterion(settings);
    Storage storage = getStorage();

    Statistics stats;
    TimeStep step = criterion.compute(pool, storage, INFTY, stats);

    // this is quite imprecise due to approximative sqrt, but it doesn't really matter for timestep estimation
    const Float factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR);
    REQUIRE(step.value == approx(factor * 3._f, 1.e-3_f));
    REQUIRE(step.id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);
    REQUIRE(stats.get<int>(StatisticsId::LIMITING_PARTICLE_IDX) == MINIMAL_PARTICLE_IDX);
    REQUIRE(stats.get<Dynamic>(StatisticsId::LIMITING_VALUE) == 12._f);
    REQUIRE(stats.get<Dynamic>(StatisticsId::LIMITING_DERIVATIVE) == 4._f);

    // modify minimal value
    IMaterial& material = storage.getMaterial(0);
    material.setRange(QuantityId::ENERGY, Interval::unbounded(), 4._f);
    step = criterion.compute(pool, storage, INFTY, stats);
    REQUIRE(step.value == approx(factor * 4._f, 1.e-3_f)); // (12+4)/4
    REQUIRE(step.id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);

    step = criterion.compute(pool, storage, 0.1_f, stats);
    REQUIRE(step.value == 0.1_f);
    REQUIRE(step.id == CriterionId::MAXIMAL_VALUE);
}

TEST_CASE("Derivative Criterion mean", "[timestepping]") {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    settings.set(RunSettingsId::TIMESTEPPING_MEAN_POWER, -8._f);
    DerivativeCriterion criterion(settings);
    Storage storage = getStorage();

    Statistics stats;
    TimeStep step = criterion.compute(pool, storage, INFTY, stats);

    // compute what the result should be
    GeneralizedMean<-8> expectedMean;
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        expectedMean.accumulate((12._f + Sph::abs(i - MINIMAL_PARTICLE_IDX)) / 4._f);
    }
    const Float factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR);
    REQUIRE(step.value == approx(factor * expectedMean.compute(), 0.02_f));

    REQUIRE(step.id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);
    REQUIRE_FALSE(stats.has(StatisticsId::LIMITING_PARTICLE_IDX));
    REQUIRE_FALSE(stats.has(StatisticsId::LIMITING_VALUE));
    REQUIRE_FALSE(stats.has(StatisticsId::LIMITING_DERIVATIVE));

    settings.set(RunSettingsId::TIMESTEPPING_MEAN_POWER, 1._f);
    // not implemented for positive powers (nor it makes sense, really)
    REQUIRE_ASSERT(DerivativeCriterion(settings));
}

TEST_CASE("Acceleration Criterion", "[timestepping]") {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    settings.set(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR, 1._f);
    AccelerationCriterion criterion(settings);
    Storage storage = getStorage();

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        dv[i] = Vector(0.2_f, 0._f, 0._f);
    }

    Statistics stats;
    TimeStep step = criterion.compute(pool, storage, INFTY, stats);
    const Float h = r[0][H];
    const Float expected4 = sqr(h) / sqr(0.2_f);
    REQUIRE(pow<4>(step.value) == approx(expected4));
    REQUIRE(step.id == CriterionId::ACCELERATION);

    step = criterion.compute(pool, storage, EPS, stats);
    REQUIRE(step.value == EPS);
    REQUIRE(step.id == CriterionId::MAXIMAL_VALUE);
}

/// \todo test multicriterion
