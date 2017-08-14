#include "timestepping/TimeStepCriterion.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Storage.h"
#include "sph/Material.h"
#include "sph/initial/Distribution.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "tests/Approx.h"

using namespace Sph;

static Storage getStorage() {
    Storage storage(getDefaultMaterial());
    HexagonalPacking distribution;
    storage.insert<Vector>(QuantityId::POSITIONS,
        OrderEnum::SECOND,
        distribution.generate(100, BlockDomain(Vector(0._f), Vector(100._f))));
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, 0._f);
    storage.getMaterial(0)->minimal(QuantityId::ENERGY) = EPS;

    const Float cs = 5._f;
    storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, cs);
    return storage;
}

TEST_CASE("Courant Criterion", "[timestepping]") {
    CourantCriterion cfl(RunSettings::getDefaults());
    const Float courantNumber = RunSettings::getDefaults().get<Float>(RunSettingsId::TIMESTEPPING_COURANT);

    Storage storage = getStorage();

    Float step;
    CriterionId id;
    tieToTuple(step, id) = cfl.compute(storage, INFTY);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    const Float h = r[0][H]; // all hs are the same
    const Float expected = courantNumber * h / cs[0];
    REQUIRE(expected == approx(step));
    REQUIRE(id == CriterionId::CFL_CONDITION);

    // get timestep limited by max value
    tieToTuple(step, id) = cfl.compute(storage, 1.e-3_f);
    REQUIRE(step == 1.e-3_f);
    REQUIRE(id == CriterionId::MAXIMAL_VALUE);
}

TEST_CASE("Derivative Criterion", "[timestepping]") {
    DerivativeCriterion criterion(RunSettings::getDefaults());
    Storage storage = getStorage();

    ArrayView<Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityId::ENERGY);
    for (Float& f : u) {
        f = 12._f; // u = 12
    }
    for (Float& f : du) {
        f = 4._f; // du/dt = 4
    }
    Float step;
    CriterionId id;
    Statistics stats;
    tieToTuple(step, id) = criterion.compute(storage, INFTY, stats);

    // this is quite imprecise due to approximative sqrt, but it doesn't really matter for timestep estimation
    const Float factor = RunSettings::getDefaults().get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
    REQUIRE(step == approx(factor * 3._f, 1.e-3_f));
    REQUIRE(id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);

    storage.getMaterial(0)->minimal(QuantityId::ENERGY) = 4._f;
    tieToTuple(step, id) = criterion.compute(storage, INFTY, stats);
    REQUIRE(step == approx(factor * 4._f, 1.e-3_f)); // (12+4)/4
    REQUIRE(id == CriterionId::DERIVATIVE);
    REQUIRE(stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY) == QuantityId::ENERGY);

    tieToTuple(step, id) = criterion.compute(storage, 0.1_f);
    REQUIRE(step == 0.1_f);
    REQUIRE(id == CriterionId::MAXIMAL_VALUE);
}

TEST_CASE("Acceleration Criterion", "[timestepping]") {
    AccelerationCriterion criterion;
    Storage storage = getStorage();

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        dv[i] = Vector(0.2_f, 0._f, 0._f);
    }

    Float step;
    CriterionId id;
    tieToTuple(step, id) = criterion.compute(storage, INFTY);
    const Float h = r[0][H];
    const Float expected4 = sqr(h) / sqr(0.2_f);
    REQUIRE(pow<4>(step) == approx(expected4));
    REQUIRE(id == CriterionId::ACCELERATION);

    tieToTuple(step, id) = criterion.compute(storage, EPS);
    REQUIRE(step == EPS);
    REQUIRE(id == CriterionId::MAXIMAL_VALUE);
}

/// \todo test multicriterion
