#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "objects/utility/ArrayUtils.h"
#include "objects/wrappers/Interval.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "quantities/Iterate.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SummationSolver.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

enum class Options {
    CHECK_ENERGY = 1 << 0,
    CHECK_MOVEMENT = 1 << 1,
};


/// Test that a gass sphere will expand and particles gain velocity in direction from center of the ball.
/// Decrease and internal energy should decrease, smoothing lenghts of all particles should increase.
/// Momentum, angular momentum and total energy should remain constant.
template <typename TSolver>
SharedPtr<Storage> solveGassBall(RunSettings settings, Flags<Options> flags) {
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-4_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
        .setFlags(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE_GRADIENT)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 10);

    TSolver solver(settings, getStandardEquations(settings));

    const Float rho0 = 10._f;
    const Float u0 = 1.e4_f;
    BodySettings body;
    body.set(BodySettingsId::DENSITY, rho0).set(BodySettingsId::ENERGY, u0);
    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getGassStorage(200, body, 1._f));
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    const Float h = r[0][H];

    // check integrals of motion

    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;
    TotalEnergy energy;
    const Vector mom0 = momentum.evaluate(*storage);
    const Vector angmom0 = angularMomentum.evaluate(*storage);
    const Float en0 = energy.evaluate(*storage);
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    REQUIRE(en0 == approx(rho0 * u0 * sphereVolume(1._f)));

    EulerExplicit timestepping(storage, settings);
    Statistics stats;
    // make few timesteps
    Size stepCnt = 0;
    for (Float t = 0._f; t < 5.e-2_f; t += timestepping.getTimeStep()) {
        timestepping.step(solver, stats);
        stepCnt++;
    }
    REQUIRE(stepCnt > 10);

    ArrayView<Float> u, rho;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    tie(u, rho) = storage->getValues<Float>(QuantityId::ENERGY, QuantityId::DENSITY);

    auto test = [&](const Size i) -> Outcome {
        if (u[i] >= 0.9_f * u0) {
            return makeFailed("Energy did not decrease: u = ", u[i]);
        }
        if (rho[i] >= 0.9_f * rho0) {
            return makeFailed("Density did not decrease: rho = ", rho[i]);
        }
        if (r[i] == Vector(0._f)) {
            return SUCCESS; // so we don't deal with this singular case
        }
        if (getLength(v[i]) == 0._f) {
            return makeFailed("Particle didn't move");
        }
        // velocity away from center => velocity is in direction of position
        const Vector v_norm = getNormalized(v[i]);
        const Vector r_norm = getNormalized(r[i]);
        if (v_norm != approx(r_norm, 1.e-1_f)) {
            // clang-format off
            return makeFailed("Particle has wrong velocity:\n"
                              "v_norm: ", v_norm, " == ", r_norm);
            // clang-format on
        }
        return SUCCESS;
    };
    if (flags.has(Options::CHECK_MOVEMENT)) {
        REQUIRE_SEQUENCE(test, 0, r.size());
    }

    if (flags.has(Options::CHECK_ENERGY)) {
        REQUIRE(momentum.evaluate(*storage) == approx(mom0, 5.e-2_f));
        REQUIRE(angularMomentum.evaluate(*storage) == approx(angmom0, 1.e-1_f));
        REQUIRE(energy.evaluate(*storage) == approx(en0, 5.e-2_f));
    }

    return storage;
}

TYPED_TEST_CASE_2("SymmetricSolver gass ball", "[solvers]", TSolver, SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_FORMULATION, FormulationEnum::STANDARD);
    solveGassBall<TSolver>(settings, Options::CHECK_ENERGY | Options::CHECK_MOVEMENT);

    settings.set(RunSettingsId::SPH_FORMULATION, FormulationEnum::BENZ_ASPHAUG);
    solveGassBall<TSolver>(settings, Options::CHECK_ENERGY | Options::CHECK_MOVEMENT);
}

TEST_CASE("SymmetricSolver asymmetric derivative", "[solvers]") {
    class AsymmetricDerivative : public IDerivative {
    public:
        explicit AsymmetricDerivative(const RunSettings& UNUSED(settings)) {}

        virtual DerivativePhase phase() const override {
            return DerivativePhase::EVALUATION;
        }

        virtual void create(Accumulated& UNUSED(results)) override {}

        virtual void initialize(const Storage& UNUSED(input), Accumulated& UNUSED(results)) override {}

        virtual void evalNeighs(const Size UNUSED(idx),
            ArrayView<const Size> UNUSED(neighs),
            ArrayView<const Vector> UNUSED(grads)) override {}
    };

    auto eq = makeTerm<Tests::SingleDerivativeMaker<AsymmetricDerivative>>();
    REQUIRE_THROWS_AS(SymmetricSolver(RunSettings::getDefaults(), eq), InvalidSetup);
}

TEST_CASE("SummationSolver gass ball", "[solvers]") {
    /// \todo why energy doesn't work?
    solveGassBall<SummationSolver>(
        RunSettings::getDefaults(), /*Options::CHECK_ENERGY | */ Options::CHECK_MOVEMENT);
}

template <typename T>
bool almostEqual(Array<T>& a1, Array<T>& a2, const Float eps = EPS) {
    if (a1.size() != a2.size()) {
        return false;
    }
    for (Size i = 0; i < a1.size(); ++i) {
        if (!Sph::almostEqual(a1[i], a2[i], eps)) {
            return false;
        }
    }
    return true;
}

TEST_CASE("Solver equivalency", "[solver]") {
    SharedPtr<Storage> st1 = solveGassBall<SymmetricSolver>(RunSettings::getDefaults(), EMPTY_FLAGS);
    SharedPtr<Storage> st2 = solveGassBall<AsymmetricSolver>(RunSettings::getDefaults(), EMPTY_FLAGS);

    for (StorageElement e2 : st2->getQuantities()) {
        Quantity& q1 = st1->getQuantity(e2.id);
        Quantity& q2 = e2.quantity;
        REQUIRE(q1.getValueEnum() == q2.getValueEnum());
        REQUIRE(q1.getOrderEnum() == q2.getOrderEnum());
    }

    iterate<VisitorEnum::ZERO_ORDER>(*st1, [st2](QuantityId id, auto& values1) {
        using Type = typename std::decay_t<decltype(values1)>::Type;
        auto& values2 = st2->getValue<Type>(id);
        REQUIRE(almostEqual(values1, values2));
    });
    iterate<VisitorEnum::FIRST_ORDER>(*st1, [st2](QuantityId id, auto& values1, auto& dt1) {
        using Type = typename std::decay_t<decltype(values1)>::Type;
        auto& values2 = st2->getValue<Type>(id);
        auto& dt2 = st2->getDt<Type>(id);
        REQUIRE(almostEqual(values1, values2));
        REQUIRE(almostEqual(dt1, dt2));
    });
    iterate<VisitorEnum::SECOND_ORDER>(*st1, [st2](QuantityId id, auto& values1, auto& dt1, auto& d2t1) {
        using Type = typename std::decay_t<decltype(values1)>::Type;
        auto& values2 = st2->getValue<Type>(id);
        auto& dt2 = st2->getDt<Type>(id);
        auto& d2t2 = st2->getD2t<Type>(id);
        REQUIRE(almostEqual(values1, values2));
        REQUIRE(almostEqual(dt1, dt2));
        REQUIRE(almostEqual(d2t1, d2t2));
    });
}
