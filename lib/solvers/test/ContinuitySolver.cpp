#include "solvers/ContinuitySolver.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/wrappers/Range.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "sph/initial/Initial.h"
#include "system/Statistics.h"
#include "timestepping/TimeStepping.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;


static RunSettings getRunSettings(SolverEnum id) {
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-4_f);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT);
    settings.set(RunSettingsId::SOLVER_TYPE, id);
    return settings;
}

/// Creates a sphere with ideal gas and non-zero pressure.
static std::shared_ptr<Storage> makeGassBall(const RunSettings& globalSettings,
    const Float rho0,
    const Float u0) {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsId::ENERGY, u0);
    bodySettings.set(BodySettingsId::ENERGY_RANGE, Range(0._f, INFTY));
    bodySettings.set(BodySettingsId::ENERGY_MIN, 0.1_f * u0);
    bodySettings.set(BodySettingsId::DENSITY, rho0);
    bodySettings.set(BodySettingsId::DENSITY_RANGE, Range(EPS, INFTY));
    bodySettings.set(BodySettingsId::DENSITY_MIN, 0.1_f * rho0);
    bodySettings.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS);
    bodySettings.set(BodySettingsId::SHEAR_MODULUS, 0._f); // effectively turns off stress tensor
    SphericalDomain domain(Vector(0._f), 1._f);
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(*storage, globalSettings);
    conds.addBody(domain, bodySettings);
    return storage;
}

/// Test that a gass sphere will expand and particles gain velocity in direction from center of the ball.
/// Decrease and internal energy should decrease, smoothing lenghts of all particles should increase.
/// Momentum, angular momentum and total energy should remain constant.
TEST_CASE("ContinuitySolver gass ball", "[solvers]") {
    RunSettings settings = getRunSettings(SolverEnum::CONTINUITY_SOLVER);
    auto solver = Factory::getSolver(settings);

    const Float rho0 = 10._f;
    const Float u0 = 1.e4_f;
    std::shared_ptr<Storage> storage = makeGassBall(settings, rho0, u0);

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
    for (Float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(*solver, stats);
        stepCnt++;
    }
    REQUIRE(stepCnt > 10);

    ArrayView<Float> u, rho;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITIONS);
    tie(u, rho) = storage->getValues<Float>(QuantityId::ENERGY, QuantityId::DENSITY);

    auto test = [&](const Size i) {
        if (u[i] > u0) {
            return makeFailed("Energy increased: u = ", u[i]);
        }
        if (rho[i] > rho0) {
            return makeFailed("Density increased: rho = ", rho[i]);
        }
        if (r[i][H] < h) {
            return makeFailed("Smoothing length decreased: rho = ", rho[i]);
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
        if (v_norm != approx(r_norm, 1.e-2_f)) {
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
