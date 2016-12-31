#include "catch.hpp"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "solvers/ContinuitySolver.h"
#include "solvers/DensityIndependentSolver.h"
#include "solvers/SummationSolver.h"
#include "sph/forces/Damage.h"
#include "sph/forces/StressForce.h"
#include "sph/forces/Yielding.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.h"

using namespace Sph;

INLINE Float smoothingFunc(const Float x, const Float x1, const Float x2) {
    const Float w = exp(-(x - 0.5_f) / 0.0005_f);
    if (x > 0.52_f) {
        return x2;
    } else if (x < 0.48_f) {
        return x1;
    }
    return (x1 * w + x2) / (1._f * w + 1._f);
}

Array<Vector> sodDistribution(const int N, Float dx, const Float eta, Abstract::Logger* logger) {
    Array<Vector> x(N);
    do {
        float actX = 0.f;
        for (Size i = 0; i < x.size(); ++i) {
            x[i][0] = actX;
            const float actDx = smoothingFunc(x[i][0], dx, dx / 0.125_f);
            actX += actDx;
            x[i][H] = eta * actDx;
        }
        ASSERT(x.size() > 0);
        if (x[x.size() - 1][0] > 1._f) {
            dx -= 0.001_f / N;
        } else {
            dx += 0.001_f / N;
        }
        logger->write(std::to_string(x[x.size() - 1][0]));
    } while (!Range(0.99f, 1.01f).contains(x[x.size() - 1][0]));
    return x;
}

TEST_CASE("Sod", "[sod]") {
    // Global settings of the problem
    Settings<GlobalSettingsIds> globalSettings = GLOBAL_SETTINGS;
    globalSettings.set(GlobalSettingsIds::RUN_NAME, std::string("Sod Shock Tube Problem"));
    globalSettings.set(GlobalSettingsIds::RUN_OUTPUT_STEP, 1);
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);
    globalSettings.set(GlobalSettingsIds::DOMAIN_CENTER, Vector(0.5_f));
    globalSettings.set(GlobalSettingsIds::DOMAIN_RADIUS, 0.5_f);
    globalSettings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::PROJECT_1D);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT);
    globalSettings.set(GlobalSettingsIds::SPH_AV_ALPHA, 1.0_f);
    globalSettings.set(GlobalSettingsIds::SPH_AV_BETA, 2.0_f);
    globalSettings.set(GlobalSettingsIds::SPH_KERNEL_ETA, 1.5_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_COURANT, 0.1_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, true);
    globalSettings.set(GlobalSettingsIds::SOLVER_TYPE, SolverEnum::SUMMATION_SOLVER);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_GRAD_P, true);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false);
    // Number of SPH particles
    const int N = 600;
    // Material properties
    Settings<BodySettingsIds> bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, N);
    bodySettings.set(BodySettingsIds::INITIAL_DISTRIBUTION, DistributionEnum::LINEAR);
    bodySettings.set(BodySettingsIds::ADIABATIC_INDEX, 1.4_f);
    bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(0.05_f, NOTHING));
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0.05_f, NOTHING));
    bodySettings.set(BodySettingsIds::DENSITY, 1._f);
    bodySettings.set(BodySettingsIds::ENERGY, 2.5_f);

    // Construct solver used in Sod shock tube
    Problem sod(globalSettings);
    InitialConditions initialConditions(sod.storage, globalSettings);
    initialConditions.addBody(SphericalDomain(Vector(0.5_f), 0.5_f), bodySettings);
    sod.solver = std::make_unique<SummationSolver<StressForce<DummyYielding, DummyDamage, StandardAV>, 1>>(globalSettings);
    /// \todo hack, recreate solver with 1 dimension
    //sod.solver = std::make_unique<DensityIndependentSolver<1>>(globalSettings);

    // Solving to t = 0.5
    sod.timeRange = Range(0._f, 0.5_f);

    // Output routines
    sod.logger = std::make_unique<StdOutLogger>();
    std::string outputDir = "sod/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
    sod.output = std::make_unique<GnuplotOutput>(outputDir,
        globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME),
        Array<QuantityKey>{
            QuantityKey::POSITIONS, QuantityKey::DENSITY, QuantityKey::PRESSURE, QuantityKey::ENERGY },
        "sod.plt");

    // Setup initial conditions of Sod Shock Tube:
    // sod.storage = std::make_shared<Storage>(bodySettings);
    Storage& storage = *sod.storage;
    // 1) setup initial positions, with different spacing in each region
    const Float eta = globalSettings.get<Float>(GlobalSettingsIds::SPH_KERNEL_ETA);
    storage.getValue<Vector>(QuantityKey::POSITIONS) = sodDistribution(N, 1._f / N, eta, sod.logger.get());

    // 2) setup initial pressure and masses of particles
    ArrayView<Vector> r;
    ArrayView<Float> p, m;
    r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    tie(p, m) = storage.getValues<Float>(QuantityKey::PRESSURE, QuantityKey::MASSES);
    for (Size i = 0; i < N; ++i) {
        p[i] = smoothingFunc(r[i][0], 1._f, 0.1_f);
        // mass = 1/N *integral density * dx
        m[i] = 0.5_f * (1._f + 0.125_f) / N;
    }

    // 3) setup density to be consistent with masses
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(globalSettings);
    finder->build(storage.getValue<Vector>(QuantityKey::POSITIONS));
    LutKernel<1> kernel = Factory::getKernel<1>(globalSettings);
    Array<NeighbourRecord> neighs;
    ArrayView<Float> rho = storage.getValue<Float>(QuantityKey::DENSITY);
    for (Size i = 0; i < N; ++i) {
        if (r[i][X] < 0.25_f) {
            rho[i] = 1._f;
        } else if (r[i][X] > 0.75_f) {
            rho[i] = 0.125_f;
        } else {
            finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs);
            rho[i] = 0._f;
            for (Size n = 0; n < neighs.size(); ++n) {
                const Size j = neighs[n].index;
                rho[i] += m[j] * kernel.value(r[i] - r[j], r[i][H]);
            }
        }
    }

    // 4) compute internal energy using equation of state
    std::unique_ptr<Abstract::Eos> eos = Factory::getEos(bodySettings);
    ArrayView<Float> u = storage.getValue<Float>(QuantityKey::ENERGY);
    for (Size i = 0; i < N; ++i) {
        u[i] = eos->getInternalEnergy(
            smoothingFunc(r[i][0], 1._f, 0.125_f), smoothingFunc(r[i][0], 1._f, 0.1_f));
    }

    // 5) compute energy per particle and energy density if we are using DISPH
    /// \todo this should be somehow computed automatically
    if (globalSettings.get<SolverEnum>(GlobalSettingsIds::SOLVER_TYPE) == SolverEnum::DENSITY_INDEPENDENT) {
        ArrayView<Float> q, e;
        tie(q, e)= storage.getValues<Float>(QuantityKey::ENERGY_DENSITY, QuantityKey::ENERGY_PER_PARTICLE);
        for (Size i = 0; i < N; ++i) {
            // update 'internal' quantities in case 'external' quantities (density, specific energy, ...) have
            // been changed outside of the solver.
            q[i] = rho[i] * u[i];
            e[i] = m[i] * u[i];
        }
    }

    // 6) setup used timestepping algorithm (this needs to be done after all quantities are allocated)
    sod.timeStepping = Factory::getTimestepping(globalSettings, sod.storage);

    // 7) run the main loop
    sod.run();
}
