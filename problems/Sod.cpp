#include "catch.hpp"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "system/Factory.h"
#include "system/Settings.h"

using namespace Sph;

TEST_CASE("Sod", "[sod]") {
    Settings<GlobalSettingsIds> globalSettings = GLOBAL_SETTINGS;
    globalSettings.set<std::string>(GlobalSettingsIds::RUN_NAME, "Sod Shock Tube Problem");
    globalSettings.set<int>(GlobalSettingsIds::DOMAIN_TYPE, int(DomainEnum::SPHERICAL));
    globalSettings.set<int>(GlobalSettingsIds::BOUNDARY, int(BoundaryEnum::PROJECT_1D));
    globalSettings.set<int>(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR,
                            int(TimesteppingEnum::PREDICTOR_CORRECTOR));
    globalSettings.set<float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-5);
    globalSettings.set<float>(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-3);
    globalSettings.set<bool>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, false);

    Problem<BasicModel<1>> sod(globalSettings);
    sod.timeRange = Range(0._f, 0.3_f);
    sod.logger    = std::make_unique<StdOutput>();

    const int N                            = 500;
    Settings<BodySettingsIds> bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, N);
    bodySettings.set(BodySettingsIds::INITIAL_DISTRIBUTION, int(DistributionEnum::LINEAR));

    SphericalDomain domain(Vector(0.5_f), 0.5_f);
    *sod.storage     = sod.model.createParticles(domain, bodySettings);
    sod.timeStepping = Factory::getTimestepping(globalSettings, sod.storage);

    /// setup initial conditions of Sod Shock Tube
    ArrayView<Vector> x = sod.storage->get<QuantityKey::R>();
    ArrayView<Float> rho, p, u, m;
    tie(rho, p, u, m) = sod.storage->get<QuantityKey::RHO, QuantityKey::P, QuantityKey::U, QuantityKey::M>();
    Float totalM = 0._f;
    for (int i = 0; i < x.size(); ++i) {
        if (x[i][0] > 0.5_f) {
            rho[i] = 0.125_f;
            p[i]   = 0.1_f;
            m[i]   = rho[i] / N;
        } else {
            rho[i] = 1._f;
            p[i]   = 1._f;
            m[i]   = rho[i] / N;
        }
        /*rho[i] = 1._f;
        m[i] = rho[i] / N;
        p[i] = Math::pow(Math::sin(Float(i) / x.size() * Math::PI), 10._f);*/
    }
    IdealGasEos eos(bodySettings.get<float>(BodySettingsIds::ADIABATIC_INDEX).get());
    eos.getInternalEnergy(rho, p, u);

    FileOutput out1("sod0.txt");
    sod.storage->save<QuantityKey::R, QuantityKey::RHO, QuantityKey::P, QuantityKey::U>(&out1, 0._f);

    /* template<int... TKey>
     struct {

     };
     std::unique_ptr<Abstract::Output> createOutput() {

         return std::make_unique
     }*/

    /// run the main loop
    sod.run();

    /// save
    FileOutput out2("sod1.txt");
    sod.storage->save<QuantityKey::R, QuantityKey::RHO, QuantityKey::P, QuantityKey::U>(&out2, 0.2_f);
}
