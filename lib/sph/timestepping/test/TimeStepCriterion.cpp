#include "sph/timestepping/TimeStepCriterion.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "quantities/Material.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Settings.h"
#include "utils/Approx.h"

using namespace Sph;

static Storage getStorage() {
    Storage storage(BodySettings::getDefaults());
    HexagonalPacking distribution;
    storage.insert<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, distribution.generate(100, BlockDomain(Vector(0._f), Vector(100._f))));
    storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY, 0._f, Range::unbounded());
    MaterialAccessor(storage).minimal(QuantityIds::ENERGY, 0) = EPS;

    const Float cs = 5._f;
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::SOUND_SPEED, cs);
    return storage;
}

TEST_CASE("Courant Criterion", "[timestepping]") {
    CourantCriterion cfl(GlobalSettings::getDefaults());
    const Float courantNumber =
        GlobalSettings::getDefaults().get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);

    Storage storage = getStorage();

    Float step;
    AllCriterionIds id;
    tieToTuple(step, id) = cfl.compute(storage, INFTY);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
    const Float h = r[0][H]; // all hs are the same
    const Float expected = courantNumber * h / cs[0];
    REQUIRE(expected == approx(step));
    REQUIRE(id == CriterionIds::CFL_CONDITION);

    // get timestep limited by max value
    tieToTuple(step, id) = cfl.compute(storage, 1.e-3_f);
    REQUIRE(step == 1.e-3_f);
    REQUIRE(id == CriterionIds::MAXIMAL_VALUE);
}

TEST_CASE("Derivative Criterion", "[timestepping]") {
    DerivativeCriterion criterion(GlobalSettings::getDefaults());
    Storage storage = getStorage();

    ArrayView<Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityIds::ENERGY);
    for (Float& f : u) {
        f = 12._f; // u = 12
    }
    for (Float& f : du) {
        f = 4._f; // du/dt = 4
    }
    Float step;
    AllCriterionIds id;
    tieToTuple(step, id) = criterion.compute(storage, INFTY);

    // this is quite imprecise due to approximative sqrt, but it doesn't really matter for timestep estimation
    const Float factor =
        GlobalSettings::getDefaults().get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
    REQUIRE(step == approx(factor * 3._f, 1.e-3_f));
    REQUIRE(id == QuantityIds::ENERGY);

    MaterialAccessor(storage).minimal(QuantityIds::ENERGY, 0) = 4._f;
    tieToTuple(step, id) = criterion.compute(storage, INFTY);
    REQUIRE(step == approx(factor * 4._f, 1.e-3_f)); // (12+4)/4
    REQUIRE(id == QuantityIds::ENERGY);

    tieToTuple(step, id) = criterion.compute(storage, 0.1_f);
    REQUIRE(step == 0.1_f);
    REQUIRE(id == CriterionIds::MAXIMAL_VALUE);
}

TEST_CASE("Acceleration Criterion", "[timestepping]") {
    AccelerationCriterion criterion;
    Storage storage = getStorage();

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        dv[i] = Vector(0.2_f, 0._f, 0._f);
    }

    Float step;
    AllCriterionIds id;
    tieToTuple(step, id) = criterion.compute(storage, INFTY);
    const Float h = r[0][H];
    const Float expected4 = sqr(h) / sqr(0.2_f);
    REQUIRE(pow<4>(step) == approx(expected4));
    REQUIRE(id == CriterionIds::ACCELERATION);

    tieToTuple(step, id) = criterion.compute(storage, EPS);
    REQUIRE(step == EPS);
    REQUIRE(id == CriterionIds::MAXIMAL_VALUE);
}

/// \todo test multicriterion
