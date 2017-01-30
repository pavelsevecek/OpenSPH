#include "solvers/ContinuitySolver.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/wrappers/Range.h"
#include "physics/Constants.h"
#include "physics/Integrals.h"
#include "solvers/SolverFactory.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Statistics.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;


static GlobalSettings getGlobalSettings(SolverEnum id) {
    GlobalSettings settings;
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-4_f);
    settings.set(GlobalSettingsIds::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::NONE);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT);
    settings.set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::NONE);
    settings.set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::NONE);
    settings.set(GlobalSettingsIds::SOLVER_TYPE, id);
    return settings;
}

/// Creates a sphere with ideal gas and non-zero pressure.
static std::shared_ptr<Storage> makeGassBall(const GlobalSettings& globalSettings,
    const Float rho0,
    const Float u0) {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    bodySettings.set(BodySettingsIds::ENERGY, u0);
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0._f, INFTY));
    bodySettings.set(BodySettingsIds::ENERGY_MIN, 0.1_f * u0);
    bodySettings.set(BodySettingsIds::DENSITY, rho0);
    bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(EPS, INFTY));
    bodySettings.set(BodySettingsIds::DENSITY_MIN, 0.1_f * rho0);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::IDEAL_GAS);
    bodySettings.set(BodySettingsIds::SHEAR_MODULUS, 0._f); // effectively turns off stress tensor
    SphericalDomain domain(Vector(0._f), 1._f);
    std::shared_ptr<Storage> storage = std::make_shared<Storage>();
    InitialConditions conds(storage, globalSettings);
    conds.addBody(domain, bodySettings);
    return storage;
}

/// Test that a gass sphere will expand and particles gain velocity in direction from center of the ball.
/// Decrease and internal energy should decrease, smoothing lenghts of all particles should increase.
/// Momentum, angular momentum and total energy should remain constant.
TEST_CASE("ContinuitySolver gass ball", "[solvers]") {
    GlobalSettings globalSettings = getGlobalSettings(SolverEnum::CONTINUITY_SOLVER);
    auto solver = getSolver(globalSettings);

    const Float rho0 = 10._f;
    const Float u0 = 1.e4_f;
    std::shared_ptr<Storage> storage = makeGassBall(globalSettings, rho0, u0);

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);
    const Float h = r[0][H];

    // check integrals of motion
    Integrals integrals;
    const Vector mom0 = integrals.getTotalMomentum(*storage);
    const Vector angmom0 = integrals.getTotalAngularMomentum(*storage);
    const Float en0 = integrals.getTotalEnergy(*storage);
    REQUIRE(mom0 == Vector(0._f));
    REQUIRE(angmom0 == Vector(0._f));
    REQUIRE(en0 == approx(rho0 * u0 * sphereVolume(1._f)));

    EulerExplicit timestepping(storage, globalSettings);
    Statistics stats;
    // make few timesteps
    Size stepCnt = 0;
    for (Float t = 0._f; t < 1._f; t += timestepping.getTimeStep()) {
        timestepping.step(*solver, stats);
        stepCnt++;
    }
    REQUIRE(stepCnt > 10);

    ArrayView<Float> u, rho;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityIds::POSITIONS);
    tie(u, rho) = storage->getValues<Float>(QuantityIds::ENERGY, QuantityIds::DENSITY);

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

    const Vector mom1 = integrals.getTotalMomentum(*storage);
    const Vector angmom1 = integrals.getTotalAngularMomentum(*storage);
    const Float en1 = integrals.getTotalEnergy(*storage);

    REQUIRE(mom1 == approx(mom0, 5.e-2_f));
    REQUIRE(angmom1 == approx(angmom0, 1.e-1_f));
    REQUIRE(en1 == approx(en0, 5.e-2_f));
}
