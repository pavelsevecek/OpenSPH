#include "catch.hpp"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "sph/forces/StressForce.h"
#include "solvers/ContinuitySolver.h"
#include "system/Factory.h"
#include "system/Settings.h"

using namespace Sph;

INLINE Float smoothingFunc(const Float x, const Float x1, const Float x2) {
    const Float w = exp(-(x - 0.5_f) / 0.001_f);
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
        for (int i = 0; i < x.size(); ++i) {
            x[i][0]           = actX;
            const float actDx = smoothingFunc(x[i][0], dx, dx / 0.125_f);
            actX += actDx;
            x[i][H] = eta * actDx;
        }
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
    globalSettings.set<std::string>(GlobalSettingsIds::RUN_NAME, "Sod Shock Tube Problem");
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);
    globalSettings.set(GlobalSettingsIds::DOMAIN_CENTER, Vector(0.5_f));
    globalSettings.set(GlobalSettingsIds::DOMAIN_RADIUS, 0.5_f);
    globalSettings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::PROJECT_1D);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT);
    globalSettings.set(GlobalSettingsIds::SPH_AV_ALPHA, 0.5_f);
    globalSettings.set(GlobalSettingsIds::SPH_AV_BETA, 1.0_f);
    globalSettings.set(GlobalSettingsIds::SPH_KERNEL_ETA, 1.7_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-5_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-5_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, true);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_GRAD_P, false);
    // Number of SPH particles
    const int N = 400;
    // Material properties
    Settings<BodySettingsIds> bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, N);
    bodySettings.set(BodySettingsIds::INITIAL_DISTRIBUTION, DistributionEnum::LINEAR);
    bodySettings.set(BodySettingsIds::ADIABATIC_INDEX, 1.4_f);
    bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(0.05_f, NOTHING));
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0.05_f, NOTHING));
    bodySettings.set(BodySettingsIds::DENSITY, 1._f);

    // Construct solver used in Sod shock tube
    Problem sod(globalSettings);
//    sod.solver = ContinuitySolver<StressForce<DummyYielding, DummyDamage, StandardDV>, 1>(globalSettings);

    // Solving to t = 0.5
    sod.timeRange = Range(0._f, 0.5_f);

    // Output routines
    sod.logger = std::make_unique<StdOutLogger>();
    sod.output = std::make_unique<GnuplotOutput>(
        "sod/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME), "sod.plt");


    // Setup initial conditions of Sod Shock Tube:
    sod.storage      = std::make_shared<Storage>(bodySettings);
    Storage& storage = *sod.storage;
    // 1) setup initial positions, with different spacing in each region
    const Float eta = globalSettings.get<Float>(GlobalSettingsIds::SPH_KERNEL_ETA);
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityKey::POSITIONS, sodDistribution(N, 1._f / N, eta, sod.logger.get()));

    // 2) setup initial pressure and masses of particles
    storage.emplaceWithFunctor<Float, OrderEnum::FIRST_ORDER>(
        QuantityKey::PRESSURE, [&](const Vector& r, const int) { return smoothingFunc(r[0], 1._f, 0.1_f); });
    // mass = 1/N *integral density * dx
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::MASSES, 0.5_f * (1._f + 0.125_f) / N);

    // 3) setup density to be consistent with masses
    std::unique_ptr<Abstract::Finder> finder = Factory::getFinder(globalSettings);
    finder->build(storage.getValue<Vector>(QuantityKey::POSITIONS));
    LutKernel<1> kernel = Factory::getKernel<1>(globalSettings);
    Array<NeighbourRecord> neighs;
    ArrayView<const Float> m  = storage.getValue<Float>(QuantityKey::MASSES);
    ArrayView<const Vector> rs = storage.getValue<Vector>(QuantityKey::POSITIONS);
    storage.emplaceWithFunctor<Float, OrderEnum::FIRST_ORDER>(
        QuantityKey::DENSITY, [&](const Vector& x, const int i) {
              if (x[X] < 0.25_f) {
                  return 1._f;
              } else if (x[X] > 0.75_f) {
                  return 0.125_f;
              }
              finder->findNeighbours(i, rs[i][H] * kernel.radius(), neighs);
              Float rho = 0._f;
              for (int n = 0; n < neighs.size(); ++n) {
                  const int j = neighs[n].index;
                  rho += m[j] * kernel.value(rs[i] - rs[j], rs[i][H]);
              }
              return rho;
            //return smoothingFunc(x[0], 1._f, 0.125_f);
        });

    // 4) compute internal energy using equation of state
    std::unique_ptr<Abstract::Eos> eos = Factory::getEos(bodySettings);
    storage.emplaceWithFunctor<Float, OrderEnum::FIRST_ORDER>(
        QuantityKey::ENERGY, [&](const Vector& r, const int) {
            const Float rho = smoothingFunc(r[0], 1._f, 0.125_f);
            const Float p   = smoothingFunc(r[0], 1._f, 0.1_f);
            return eos->getInternalEnergy(rho, p);
        });

    // 5) setup other quantities needed by model (sounds speed, ...)
    SphericalDomain domain(Vector(0.5_f), 0.5_f);
    ///sod.solver->setQuantities(storage, domain, bodySettings);
    /// \todo initial conditions - iterate over solver quantities

    // 6) setup used timestepping algorithm (this needs to be done after all quantities are allocated)
    sod.timeStepping = Factory::getTimestepping(globalSettings, sod.storage);

    // 7) run the main loop
    sod.run();
}
