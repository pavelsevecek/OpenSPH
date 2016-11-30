#include "catch.hpp"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "solvers/SummationSolver.h"
#include "system/Factory.h"
#include "system/Settings.h"

using namespace Sph;

TEST_CASE("Sod", "[sod]") {
    Settings<GlobalSettingsIds> globalSettings = GLOBAL_SETTINGS;
    globalSettings.set<std::string>(GlobalSettingsIds::RUN_NAME, "Sod Shock Tube Problem");
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);
    globalSettings.set(GlobalSettingsIds::DOMAIN_CENTER, Vector(0.5_f));
    globalSettings.set(GlobalSettingsIds::DOMAIN_RADIUS, 0.5_f);
    globalSettings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::PROJECT_1D);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT);
    globalSettings.set(GlobalSettingsIds::AV_ALPHA, 1.0_f);
    globalSettings.set(GlobalSettingsIds::AV_BETA, 2.0_f);
    globalSettings.set<Float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-4_f);
    globalSettings.set<Float>(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-3_f);
    globalSettings.set<bool>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, false);

    Problem<ContinuitySolver<1>> sod(globalSettings);
    sod.timeRange = Range(0._f, 0.5_f);
    sod.logger    = std::make_unique<StdOutLogger>();
    sod.output    = std::make_unique<GnuplotOutput>("sod/" +
                                                     globalSettings.get<std::string>(
                                                         GlobalSettingsIds::RUN_OUTPUT_NAME),
                                                 "sod.plt");

    const int N                            = 1000;
    Settings<BodySettingsIds> bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, N);
    bodySettings.set(BodySettingsIds::INITIAL_DISTRIBUTION, int(DistributionEnum::LINEAR));
    bodySettings.set(BodySettingsIds::ADIABATIC_INDEX, 1.4_f);
    bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(0.05_f, NOTHING));
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(0.05_f, NOTHING));
    bodySettings.set(BodySettingsIds::DENSITY, 1._f);

    SphericalDomain domain(Vector(0.5_f), 0.5_f);
    *sod.storage     = sod.model.createParticles(domain, bodySettings);
    sod.timeStepping = Factory::getTimestepping(globalSettings, sod.storage);

    /// setup initial conditions of Sod Shock Tube
    ArrayView<Vector> x = sod.storage->get<QuantityKey::R>();
    ArrayView<Float> rho, p, u, m;
    tie(rho, p, u, m) = sod.storage->get<QuantityKey::RHO, QuantityKey::P, QuantityKey::U, QuantityKey::M>();
    Float totalM = 0._f;
    auto func    = [](const Float x, const Float x1, const Float x2) {
        const Float w = exp(-(x - 0.5_f) / 0.001_f);
        if (x > 0.55_f) {
            return x2;
        } else if (x < 0.45_f) {
            return x1;
        }
        return (x1 * w + x2) / (1._f * w + 1._f);
    };


    Float actX = 0._f;
    for (int i = 0; i < x.size(); ++i) {
       /* x[i][0] = actX;
        if (actX < 0.5_f) {
            actX += 0.0018750_f;
            x[i][H] = 1.5_f * 0.0018750_f;
        } else {
            actX += 0.0075000_f;
            x[i][H] = 1.5_f * 0.0075000_f;
        }*/
        rho[i] = func(x[i][0], 1._f, 0.125_f);
        p[i]   = func(x[i][0], 1._f, 0.1_f);
        m[i]   = /*0.0018750_f; //*/ rho[i] / N;
    }

    IdealGasEos eos(bodySettings.get<Float>(BodySettingsIds::ADIABATIC_INDEX));
    eos.getInternalEnergy(rho, p, u);


    /// run the main loop
    sod.run();
}