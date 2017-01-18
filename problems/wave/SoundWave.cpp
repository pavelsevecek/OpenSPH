#include "catch.hpp"
#include "geometry/Domain.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "system/Settings.h"

using namespace Sph;


TEST_CASE("StressForce Soundwave", "[stressforce]") {
    GlobalSettings settings = GLOBAL_SETTINGS;
    settings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::GHOST_PARTICLES);
    /*settings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::BLOCK);
    settings.set(GlobalSettingsIds::DOMAIN_SIZE, Vector(1._f, 1._f, 20.05_f));*/
    settings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::CYLINDER);
    settings.set(GlobalSettingsIds::DOMAIN_HEIGHT, 20._f);
    settings.set(GlobalSettingsIds::DOMAIN_RADIUS, 0.5_f);
    settings.set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL);
    settings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false);
    settings.set(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, true);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f);
    settings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-4_f);
    settings.set(GlobalSettingsIds::RUN_OUTPUT_STEP, 1);
    Problem p(settings);
    InitialConditions conds(p.storage, settings);

    BodySettings bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
    bodySettings.set(BodySettingsIds::DENSITY, 1000._f);
    bodySettings.set(BodySettingsIds::DENSITY_MIN, 1._f);
    bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(10._f, Extended::infinity()));
    const Float u0 = 1.e4_f;
    bodySettings.set(BodySettingsIds::ENERGY, u0);
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(10._f, Extended::infinity()));
    bodySettings.set(BodySettingsIds::ENERGY_MIN, 1._f);
    //conds.addBody(BlockDomain(Vector(0._f), Vector(1._f, 1._f, 20._f)), bodySettings);
    conds.addBody(CylindricalDomain(Vector(0._f), 0.5_f, 20._f, true), bodySettings);
    p.timeStepping = Factory::getTimestepping(settings, p.storage);
    p.output = std::make_unique<GnuplotOutput>("out_%d.txt",
        "wave",
        Array<QuantityIds>{
            QuantityIds::POSITIONS, QuantityIds::DENSITY, QuantityIds::PRESSURE, QuantityIds::ENERGY },
        "wave.plt");

    p.timeRange = Range(0._f, 0.1_f);
    StdOutLogger logger;

    const Float gamma = bodySettings.get<Float>(BodySettingsIds::ADIABATIC_INDEX);
    const Float cs = Sph::sqrt(gamma * (gamma - 1._f) * u0);
    logger.write("Sound speed = ", cs);
    // find equilibrium state - in 0.1s, soundwave should propagate at least 5 times across the cylinder
    p.run();
}
