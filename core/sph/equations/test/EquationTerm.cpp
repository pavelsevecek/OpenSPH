#include "sph/equations/EquationTerm.h"
#include "catch.hpp"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/PerElementWrapper.h"
#include "sph/equations/HelperTerms.h"
#include "sph/equations/av/Standard.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

/// \todo Move to some common place (utils.h)

struct TestDerivative : public ISymmetricDerivative {
    static bool initialized;
    static bool created;

    ArrayView<Size> flags;

    TestDerivative() = default;

    // needed for some generic functions creating derivatives from settings
    explicit TestDerivative(const RunSettings& UNUSED(settings)) {}

    virtual void create(Accumulated& results) override {
        results.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, BufferSource::UNIQUE);
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

    virtual void initialize(IScheduler&, Storage&, const Float) override {
        flags.set(Status::INITIALIZED);
    }

    virtual void finalize(IScheduler&, Storage&, const Float) override {
        flags.set(Status::FINALIZED);
    }

    virtual void create(Storage&, IMaterial&) const override {
        flags.set(Status::STORAGE_CREATED);
    }
};

TEST_CASE("Setting derivatives", "[equationterm]") {
    TestDerivative::initialized = false;
    Tests::SingleDerivativeMaker<TestDerivative> eq;
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

    EquationHolder sum = std::move(eqs) + makeTerm<NeighborCountTerm>() +
                         makeTerm<AdaptiveSmoothingLength>(RunSettings::getDefaults());
    REQUIRE(sum.getTermCnt() == 3);
}

TEST_CASE("EquationHolder contains", "[equationterm]") {
    EquationHolder eqs;
    eqs += makeTerm<PressureForce>();
    REQUIRE(eqs.contains<PressureForce>());
    REQUIRE_FALSE(eqs.contains<TestEquation>());
}

TEMPLATE_TEST_CASE("TestEquation", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10);
    Statistics stats;
    SharedPtr<TestEquation> eq = makeShared<TestEquation>();
    EquationHolder equations(eq);
    equations += makeTerm<Tests::SingleDerivativeMaker<TestDerivative>>() + makeTerm<ConstSmoothingLength>();

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    TestType solver(pool, RunSettings::getDefaults(), std::move(equations));
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

TEMPLATE_TEST_CASE("NeighborCount", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    const Size N = storage.getParticleCnt();
    Statistics stats;
    EquationHolder equations;
    equations += makeTerm<ConstSmoothingLength>();
    TestType solver(pool, RunSettings::getDefaults(), std::move(equations));
    solver.create(storage, storage.getMaterial(0));

    solver.integrate(storage, stats);

    ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);

    REQUIRE(neighCnts.size() == N);
    // count neighbors manually and compare
    UniformGridFinder finder;
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder.build(pool, r);
    const Float radius = Factory::getKernel<3>(RunSettings::getDefaults()).radius();
    Array<NeighborRecord> neighs;
    auto test = [&](Size i) -> Outcome {
        const Size cnt = finder.findAll(i, r[i][H] * radius, neighs);
        if (cnt != neighCnts[i] + 1) {
            // +1 for the particle itself
            return makeFailed("Incorrect neighbor count for particle ", i, "\n", cnt, " == ", neighCnts[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEMPLATE_TEST_CASE("Div v of position vectors", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    // test case checking that div r = 3
    Storage storage = Tests::getStorage(10000);
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
    Tests::computeField<VelocityDivergence<CenterDensityDiscr>, TestType>(
        storage, [](const Vector& r) { return r; });

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

TEMPLATE_TEST_CASE("Grad v of const field", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    Tests::computeField<VelocityGradient<CenterDensityDiscr>, TestType>(storage, [](const Vector&) { //
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

TEMPLATE_TEST_CASE("Grad v of position vector", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    Tests::computeField<VelocityGradient<CenterDensityDiscr>, TestType>(
        storage, [](const Vector& r) { return r; });

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

TEMPLATE_TEST_CASE("Grad v of non-trivial field", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    Tests::computeField<VelocityGradient<CenterDensityDiscr>, TestType>(storage, [](const Vector& r) { //
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

template <typename TRotation, typename TSolver>
static void testRotation(QuantityId id) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<Vector>(id, OrderEnum::ZERO, Vector(0._f));
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, 1._f);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);
    Tests::computeField<TRotation, TSolver>(storage, [](const Vector& r) { //
        return Vector(r[2] * sqr(r[1]), r[0] + 0.5_f * r[2], sin(r[1]));
    });

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> rotV = storage.getValue<Vector>(id);
    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            // skip test by reporting success
            return SUCCESS;
        }
        const Float y = r[i][Y];
        const Float z = r[i][Z];
        Vector expected(cos(y) - 0.5_f, sqr(y) - 0._f, 1._f - 2._f * z * y);
        if (rotV[i] != approx(expected, 0.03_f)) {
            // clang-format off
                 return makeFailed("Invalid rot v"
                                   "\n r = ", r[i],
                                   "\n rot v = ", rotV[i],
                                   "\n expected = ", expected);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEMPLATE_TEST_CASE("Rot v", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    testRotation<VelocityRotation<CenterDensityDiscr>, TestType>(QuantityId::VELOCITY_ROTATION);
}

namespace {

class VelocityLaplacian : public DerivativeTemplate<VelocityLaplacian> {
private:
    ArrayView<const Vector> r, v;
    ArrayView<const Float> m, rho;
    ArrayView<Vector> divGradV;

public:
    explicit VelocityLaplacian(const RunSettings& settings)
        : DerivativeTemplate<VelocityLaplacian>(settings) {}

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<Vector>(QuantityId::VELOCITY_LAPLACIAN, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        ArrayView<const Vector> dummy;
        tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
        divGradV = results.getBuffer<Vector>(QuantityId::VELOCITY_LAPLACIAN, OrderEnum::ZERO);
    }

    INLINE bool additionalEquals(const VelocityLaplacian& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const Vector dgv = laplacian(v[j] - v[i], grad, r[j] - r[i]);
        divGradV[i] += m[j] / rho[j] * dgv;
        if (Symmetrize) {
            divGradV[j] -= m[i] / rho[i] * dgv;
        }
    }
};

} // namespace

TEMPLATE_TEST_CASE("Laplacian vector", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    Storage storage = Tests::getStorage(10000);
    storage.insert<Vector>(QuantityId::VELOCITY_LAPLACIAN, OrderEnum::ZERO, Vector(0._f));
    Tests::computeField<VelocityLaplacian, TestType>(storage, [](const Vector& r) { //
        return Vector(sqr(r[0]) * sqr(r[1]), exp(r[0]) + 0.5_f * cos(r[2]), sin(r[2]));
    });
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> divGradV = storage.getValue<Vector>(QuantityId::VELOCITY_LAPLACIAN);

    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            // skip test by reporting success
            return SUCCESS;
        }
        const Float x = r[i][X];
        const Float y = r[i][Y];
        const Float z = r[i][Z];
        Vector expected = Vector(2._f * sqr(y), exp(x), 0._f)       // d^2v/dx^2
                          + Vector(2._f * sqr(x), 0._f, 0._f)       // d^2v/dy^2
                          + Vector(0._f, -0.5_f * cos(z), -sin(z)); // d^2v/dz^2
        if (divGradV[i] != approx(expected, 0.05_f)) {
            // clang-format off
            return makeFailed("Invalid laplacian"
                              "\n r = ", r[i],
                              "\n Delta v = ", divGradV[i],
                              "\n expected = ", expected);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

namespace {

class GradientOfVelocityDivergence : public DerivativeTemplate<GradientOfVelocityDivergence> {
private:
    ArrayView<const Vector> r, v;
    ArrayView<const Float> m, rho;
    ArrayView<Vector> gradDivV;

public:
    explicit GradientOfVelocityDivergence(const RunSettings& settings)
        : DerivativeTemplate<GradientOfVelocityDivergence>(settings) {}

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<Vector>(
            QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        ArrayView<const Vector> dummy;
        tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
        tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
        gradDivV = results.getBuffer<Vector>(QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, OrderEnum::ZERO);
    }

    INLINE bool additionalEquals(const GradientOfVelocityDivergence& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const Vector gdv = gradientOfDivergence(v[j] - v[i], grad, r[j] - r[i]);
        gradDivV[i] += m[j] / rho[j] * gdv;
        if (Symmetrize) {
            gradDivV[j] -= m[i] / rho[i] * gdv;
        }
    }
};

} // namespace

TEMPLATE_TEST_CASE("Gradient of divergence", "[equationterm]", SymmetricSolver<3>, AsymmetricSolver) {
    SKIP_TEST;

    Storage storage = Tests::getStorage(10000);
    storage.insert<Vector>(QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, OrderEnum::ZERO, Vector(0._f));
    Tests::computeField<GradientOfVelocityDivergence, TestType>(storage, [](const Vector& r) { //
        return Vector(sqr(r[X]) * sqr(r[Y]), exp(r[X]) + 0.5_f * cos(r[Z]), sin(r[Z]));
    });
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> gradDivV = storage.getValue<Vector>(QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE);

    auto test = [&](const Size i) -> Outcome {
        if (getLength(r[i]) > 0.7_f) {
            // skip test by reporting success
            return SUCCESS;
        }
        const Float x = r[i][X];
        const Float y = r[i][Y];
        const Float z = r[i][Z];
        // div v = 2*x*y^2 + 0 + cos(z)
        Vector expected(2._f * sqr(y), 4._f * x * y, -sin(z));
        if (gradDivV[i] != approx(expected, 0.05_f)) {
            // clang-format off
            return makeFailed("Invalid gradient of divergence"
                              "\n r = ", r[i],
                              "\n grad div v = ", gradDivV[i],
                              "\n expected = ", expected);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}


TEST_CASE("Strain rate correction", "[equationterm]") {
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    Storage storage = Tests::getSolidStorage(1000, body);
    storage.insert<SymmetricTensor>(
        QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor::identity());

    RunSettings settings;
    settings.set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true);

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    AsymmetricSolver solver(pool, settings, getStandardEquations(settings));
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
        if (corr[i] == SymmetricTensor::identity()) {
            return makeFailed("Correction tensor is 'exactly' identity matrix: \nC[i] == ", corr[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, corr.size());

    // all damaged particles -> we should get identity matrix
    storage.getValue<Float>(QuantityId::DAMAGE).fill(1._f);
    solver.integrate(storage, stats);
    corr = storage.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);

    auto test2 = [corr](Size i) -> Outcome {
        // currently results in zero, may change in the future
        if (corr[i] != SymmetricTensor::identity()) {
            return makeFailed(
                "Correction tensors of damaged particle is not identity matrix:\nC[i] == ", corr[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test2, 0, corr.size());

    // check that inverted singular matrix is something reasonable
    // we get 'singular' matrix (matrix with very small determinant) by settings masses to EPS
    storage.getValue<Float>(QuantityId::DAMAGE).fill(0._f);
    storage.getValue<Float>(QuantityId::MASS).fill(EPS);
    solver.integrate(storage, stats);
    corr = storage.getValue<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);

    auto test3 = [corr](Size i) -> Outcome {
        // currently results in identity matrix, may change in the future
        if (corr[i] != SymmetricTensor::identity()) {
            return makeFailed("Incorrect inversion of singular matrix:\nC[i] == ", corr[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test3, 0, corr.size());
}
