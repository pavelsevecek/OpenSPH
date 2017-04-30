#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/wrappers/Range.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "solvers/ContinuitySolver.h"
#include "solvers/SummationSolver.h"
#include "sph/initial/Initial.h"
#include "system/Statistics.h"
#include "timestepping/TimeStepping.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;


/// Test that a gass sphere will expand and particles gain velocity in direction from center of the ball.
/// Decrease and internal energy should decrease, smoothing lenghts of all particles should increase.
/// Momentum, angular momentum and total energy should remain constant.
template <typename TSolver>
void solveGassBall() {
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-4_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false)
        .set(RunSettingsId::MODEL_FORCE_PRESSURE_GRADIENT, true)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 10);

    TSolver solver(settings);

    const Float rho0 = 10._f;
    const Float u0 = 1.e4_f;
    std::shared_ptr<Storage> storage =
        std::make_shared<Storage>(Tests::getGassStorage(200, BodySettings::getDefaults(), 1._f, rho0, u0));
    solver.create(*storage, storage->getMaterial(0));

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITIONS);
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
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITIONS);
    tie(u, rho) = storage->getValues<Float>(QuantityId::ENERGY, QuantityId::DENSITY);

    auto test = [&](const Size i) {
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
    REQUIRE_SEQUENCE(test, 0, r.size());

    REQUIRE(momentum.evaluate(*storage) == approx(mom0, 5.e-2_f));
    REQUIRE(angularMomentum.evaluate(*storage) == approx(angmom0, 1.e-1_f));
    REQUIRE(energy.evaluate(*storage) == approx(en0, 5.e-2_f));
}

TEST_CASE("ContinuitySolver gass ball", "[solvers]") {
    solveGassBall<ContinuitySolver>();
}

TEST_CASE("SummationSolver gass ball", "[solvers]") {
    solveGassBall<SummationSolver>();
}
