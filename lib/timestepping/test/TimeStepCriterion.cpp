#include "timestepping/TimeStepCriterion.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

const Size MINIMAL_PARTICLE_IDX = 53; // just something non-trivial

static Storage getStorage() {
    Storage storage(getDefaultMaterial());
    HexagonalPacking distribution;
    storage.insert<Vector>(QuantityId::POSITION,
        OrderEnum::SECOND,
        distribution.generate(100, BlockDomain(Vector(0._f), Vector(100._f))));
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
        u[i] = 12._f + abs(MINIMAL_PARTICLE_IDX - i); // minimal value is 12 for particle 53
        du[i] = 4._f;                                 // du/dt = 4
    }

    return storage;
}

TEST_CASE("Courant Criterion", "[timestepping]") {
    CourantCriterion cfl(RunSettings::getDefaults());
    const Float courantNumber = RunSettings::getDefaults().get<Float>(RunSettingsId::TIMESTEPPING_COURANT_NUMBER);

    Storage storage = getStorage();

    Statistics stats;
    Float step;
    CriterionId id;
    tieToTuple(step, id) = cfl.compute(storage, INFTY, stats);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    const Float h = r[0][H]; // all hs are the same
    const Float expected = courantNumber * h / cs[0];
    REQUIRE(expected == approx(step));
    REQUIRE(id == CriterionId::CFL_CONDITION);

    // get timestep limited by max value
    tieToTuple(step, id) = cfl.compute(storage, 1.e-3_f, stats);
    REQUIRE(step == 1.e-3_f);
    REQUIRE(id == CriterionId::MAXIMAL_VALUE);
}

TEST_CASE("Derivative Criterion minimum", "[timestepping]") {
    RunSettings settings;
    DerivativeCriterion criterion(settings);
    Storage storage = getStorage();

    Float step;
    CriterionId id;
    Statistics stats;
    tieToTuple(step, id) = criterion.compute(storage, INFTY, stats);

    // this is quite imprecise due to approximative sqrt, but it doesn't really matter for timestep estimation
    const Float factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
    REQUIRE(step == approx(factor * 3._f, 1.e-3_f));
    REQUIRE(id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);
    REQUIRE(stats.get<int>(StatisticsId::LIMITING_PARTICLE_IDX) == MINIMAL_PARTICLE_IDX);
    REQUIRE(stats.get<Dynamic>(StatisticsId::LIMITING_VALUE) == 12._f);
    REQUIRE(stats.get<Dynamic>(StatisticsId::LIMITING_DERIVATIVE) == 4._f);

    // modify minimal value
    IMaterial& material = storage.getMaterial(0);
    material.setRange(QuantityId::ENERGY, Interval::unbounded(), 4._f);
    tieToTuple(step, id) = criterion.compute(storage, INFTY, stats);
    REQUIRE(step == approx(factor * 4._f, 1.e-3_f)); // (12+4)/4
    REQUIRE(id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);

    tieToTuple(step, id) = criterion.compute(storage, 0.1_f, stats);
    REQUIRE(step == 0.1_f);
    REQUIRE(id == CriterionId::MAXIMAL_VALUE);
}

TEST_CASE("Derivative Criterion mean", "[timestepping]") {
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_MEAN_POWER, -8._f);
    DerivativeCriterion criterion(settings);
    Storage storage = getStorage();

    Float step;
    CriterionId id;
    Statistics stats;
    tieToTuple(step, id) = criterion.compute(storage, INFTY, stats);

    // compute what the result should be
    GeneralizedMean<-8> expectedMean;
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        expectedMean.accumulate((12._f + abs(i - MINIMAL_PARTICLE_IDX)) / 4._f);
    }
    const Float factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
    REQUIRE(step == approx(factor * expectedMean.compute(), 0.02_f));

    REQUIRE(id == CriterionId::DERIVATIVE);
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
    settings.set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 1._f);
    AccelerationCriterion criterion(settings);
    Storage storage = getStorage();

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        dv[i] = Vector(0.2_f, 0._f, 0._f);
    }

    Statistics stats;
    Float step;
    CriterionId id;
    tieToTuple(step, id) = criterion.compute(storage, INFTY, stats);
    const Float h = r[0][H];
    const Float expected4 = sqr(h) / sqr(0.2_f);
    REQUIRE(pow<4>(step) == approx(expected4));
    REQUIRE(id == CriterionId::ACCELERATION);

    tieToTuple(step, id) = criterion.compute(storage, EPS, stats);
    REQUIRE(step == EPS);
    REQUIRE(id == CriterionId::MAXIMAL_VALUE);
}

/// \todo test multicriterion
