#include "sph/equations/EquationTerm.h"
#include "catch.hpp"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/PerElementWrapper.h"
#include "sph/equations/av/Standard.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/ContinuitySolver.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

/// \todo Move to some common place (utils.h)

struct TestDerivative : public SymmetricDerivative {
    static bool initialized;
    static bool created;

    ArrayView<Size> flags;

    virtual void create(Accumulated& results) override {
        results.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO);
        created = true;
    }

    virtual void initialize(const Storage& UNUSED(input), Accumulated& results) override {
        flags = results.getBuffer<Size>(QuantityId::FLAG, OrderEnum::ZERO);
        initialized = true;
    }

    virtual void evalNeighs(const Size idx,
        ArrayView<const Size> UNUSED(neighs),
        ArrayView<const Vector> UNUSED(grads)) override {
        flags[idx]++;
    }

    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> UNUSED(neighs),
        ArrayView<const Vector> UNUSED(grads)) override {
        flags[idx]++;
    }
};
bool TestDerivative::initialized = false;
bool TestDerivative::created = false;

struct TestEquation : public IEquationTerm {
    enum class Status {
        STORAGE_CREATED = 1 << 0,
        DERIVATIVES_SET = 1 << 1,
        INITIALIZED = 1 << 2,
        FINALIZED = 1 << 3,
    };
    mutable Flags<Status> flags = EMPTY_FLAGS;

    virtual void setDerivatives(DerivativeHolder&, const RunSettings&) override {
        flags.set(Status::DERIVATIVES_SET);
    }

    virtual void initialize(Storage&) override {
        flags.set(Status::INITIALIZED);
    }

    virtual void finalize(Storage&) override {
        flags.set(Status::FINALIZED);
    }

    virtual void create(Storage&, IMaterial&) const override {
        flags.set(Status::STORAGE_CREATED);
    }
};

TEST_CASE("Setting derivatives", "[equationterm]") {
    TestDerivative::initialized = false;
    Tests::DerivativeWrapper<TestDerivative> eq;
    DerivativeHolder derivatives;
    eq.setDerivatives(derivatives, RunSettings::getDefaults());
    Storage storage;
    // add some dummy quantity to set particle count
    storage.insert<Size>(QuantityId::DAMAGE, OrderEnum::FIRST, Array<Size>{ 1, 2, 3, 4, 5 });
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 2);

    // initialize, creating buffers and settings up arrayviews for derivatives
    derivatives.initialize(storage);
    derivatives.getAccumulated().store(storage);
    REQUIRE(storage.getParticleCnt() == 5);
    REQUIRE(TestDerivative::initialized);
}

TEST_CASE("EquationHolder operators", "[equationterm]") {
    EquationHolder eqs;
    REQUIRE(eqs.getTermCnt() == 0);
    eqs += makeTerm<PressureForce>();
    REQUIRE(eqs.getTermCnt() == 1);

    EquationHolder sum = std::move(eqs) + makeTerm<NeighbourCountTerm>() +
                         makeTerm<AdaptiveSmoothingLength>(RunSettings::getDefaults());
    REQUIRE(sum.getTermCnt() == 3);
}

TEST_CASE("EquationHolder contains", "[equationterm]") {
    EquationHolder eqs;
    eqs += makeTerm<PressureForce>();
    REQUIRE(eqs.contains<PressureForce>());
    REQUIRE_FALSE(eqs.contains<TestEquation>());
}

TYPED_TEST_CASE_2("TestEquation", "[equationterm]", TSolver, SymmetricSolver, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10);
    const Size N = storage.getParticleCnt();
    Statistics stats;
    SharedPtr<TestEquation> eq = makeShared<TestEquation>();
    EquationHolder equations(eq);
    equations += makeTerm<Tests::DerivativeWrapper<TestDerivative>>() + makeTerm<ConstSmoothingLength>();

    TSolver solver(RunSettings::getDefaults(), std::move(equations));
    REQUIRE(eq->flags == TestEquation::Status::DERIVATIVES_SET);

    solver.create(storage, storage.getMaterial(0));
    REQUIRE(eq->flags.has(TestEquation::Status::STORAGE_CREATED));
    REQUIRE_FALSE(eq->flags.hasAny(TestEquation::Status::INITIALIZED, TestEquation::Status::FINALIZED));

    solver.integrate(storage, stats);
    REQUIRE(eq->flags.hasAll(TestEquation::Status::INITIALIZED, TestEquation::Status::FINALIZED));

    ArrayView<Size> cnts = storage.getValue<Size>(QuantityId::FLAG);
    REQUIRE(cnts.size() == 10);
    // test equations only counts execution count, derivative should be executed once for each particle
    REQUIRE(perElement(cnts) == 1);
}

TYPED_TEST_CASE_2("NeighbourCount", "[equationterm]", TSolver, SymmetricSolver, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    const Size N = storage.getParticleCnt();
    Statistics stats;
    EquationHolder equations;
    equations += makeTerm<ConstSmoothingLength>();
    TSolver solver(RunSettings::getDefaults(), std::move(equations));
    solver.create(storage, storage.getMaterial(0));

    solver.integrate(storage, stats);

    ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);

    REQUIRE(neighCnts.size() == N);
    // count neighbours manually and compare
    UniformGridFinder finder;
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder.build(r);
    const Float radius = Factory::getKernel<3>(RunSettings::getDefaults()).radius();
    Array<NeighbourRecord> neighs;
    auto test = [&](Size i) -> Outcome {
        const Size cnt = finder.findNeighbours(i, r[i][H] * radius, neighs, EMPTY_FLAGS);
        if (cnt != neighCnts[i] + 1) {
            // +1 for the particle itself
            return makeFailed("Incorrect neighbour count for particle ", i, "\n", cnt, " == ", neighCnts[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TYPED_TEST_CASE_2("Div v of position vectors", "[equationterm]", TSolver, SymmetricSolver, AsymmetricSolver) {
    // test case checking that div r = 3
    Storage storage = Tests::getStorage(10000);
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
    Tests::computeField<VelocityDivergence, TSolver>(storage, [](const Vector& r) { return r; });

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    REQUIRE(divv.size() == r.size());

    auto test = [&](const Size i) -> Outcome {
        // particles on boundary have different velocity divergence, check only particles inside
        if (getLength(r[i]) > 0.7_f) {
            return SUCCESS;
        }
        if (divv[i] != approx(3._f, 0.03_f)) {
            // clang-format off
            return makeFailed("Incorrect velocity divergence: \n",
                              "divv: ", divv[i], " == -3", "\n particle: r = ", r[i]);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TYPED_TEST_CASE_2("Grad v of const field", "[equationterm]", TSolver, SymmetricSolver, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    Tests::computeField<VelocityGradient, TSolver>(storage, [](const Vector&) { //
        return Vector(2._f, 3._f, -1._f);
    });

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
    auto test = [&](const Size i) -> Outcome {
        // here we ALWAYS subtract two equal values, so the result should be zero EXACTLY
        if (gradv[i] != SymmetricTensor::null()) {
            // clang-format off
            return makeFailed("Invalid grad v"
                              "\n r = ", r[i],
                              "\n grad v = ", gradv[i],
                              "\n expected = ", SymmetricTensor::null());
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, gradv.size());
}

TYPED_TEST_CASE_2("Grad v of position vector", "[equationterm]", TSolver, SymmetricSolver, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    Tests::computeField<VelocityGradient, TSolver>(storage, [](const Vector& r) { return r; });

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            return SUCCESS;
        }
        if (gradv[i] != approx(SymmetricTensor::identity(), 0.05_f)) {
            // clang-format off
            return makeFailed("Invalid grad v"
                              "\n r = ", r[i],
                              "\n grad v = ", gradv[i],
                              "\n expected = ", SymmetricTensor::identity());
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TYPED_TEST_CASE_2("Grad v of non-trivial field",
    "[equationterm]",
    TSolver,
    SymmetricSolver,
    AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    Tests::computeField<VelocityGradient, TSolver>(storage, [](const Vector& r) { //
        return Vector(r[0] * sqr(r[1]), r[0] + 0.5_f * r[2], sin(r[2]));
    });

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            // skip test by reporting success
            return SUCCESS;
        }
        // gradient of velocity field
        const Float x = r[i][X];
        const Float y = r[i][Y];
        const Float z = r[i][Z];
        SymmetricTensor expected(
            Vector(sqr(y), 0._f, cos(z)), Vector(0.5_f * (1._f + 2._f * x * y), 0._f, 0.25_f));
        if (gradv[i] != approx(expected, 0.05_f)) {
            // clang-format off
            return makeFailed("Invalid grad v"
                              "\n r = ", r[i],
                              "\n grad v = ", gradv[i],
                              "\n expected = ", expected);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Strain rate correction", "[equationterm]") {
    BodySettings body;
    Storage storage = Tests::getSolidStorage(1000, body);
    storage.insert<SymmetricTensor>(
        QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor::identity());
    // add necessary quantities
    /*storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, 1._f);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, 0._f);
    storage.insert<SymmetricTensor>(
        QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST,
    TracelessTensor::null());*/

    RunSettings settings;
    settings.set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true);

    AsymmetricSolver solver(settings, getStandardEquations(settings));
    solver.create(storage, storage.getMaterial(0));
    Statistics stats;
    solver.integrate(storage, stats);

    ArrayView<SymmetricTensor> corr =
        storage.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);

    auto test1 = [corr](Size i) -> Outcome {
        // should be alpha * Id (very roughly)
        const Float det = corr[i].determinant();
        if (det > 20._f || det < 0.5_f) {
            return makeFailed("Invalid correction tensor: C[i] == ", corr[i]);
        }
        const SymmetricTensor expected = root<3>(det) * SymmetricTensor::identity();
        if (corr[i].diagonal() != approx(expected.diagonal(), 0.25_f)) {
            return makeFailed("Invalid diagonal part of correction tensor: \nC[i] == ",
                corr[i].diagonal(),
                "\nexpected == ",
                expected.diagonal());
        }
        if (corr[i].offDiagonal() != approx(expected.offDiagonal(), 0.3_f * root<3>(det))) {
            return makeFailed("Invalid off-diagonal part of correction tensor: \nC[i] == ",
                corr[i].offDiagonal(),
                "\nexpected == ",
                expected.offDiagonal());
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, corr.size());

    // check that inverted singular matrix is something reasonable
    // we get singular matrix by settings masses to zero
    storage.getValue<Float>(QuantityId::MASS).fill(0._f);

    solver.integrate(storage, stats);
    corr = storage.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);

    auto test2 = [corr](Size i) -> Outcome {
        // currently results in identity, may change in the future
        if (corr[i] != SymmetricTensor::identity()) {
            return makeFailed("Incorrect inversion of singular matrix:\nC[i] == ", corr[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, corr.size());
}
