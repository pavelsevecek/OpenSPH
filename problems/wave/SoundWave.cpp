#include "catch.hpp"
#include "geometry/Domain.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "system/Settings.h"

using namespace Sph;


TEST_CASE("StressForce Soundwave", "[stressforce]") {
    GlobalSettings settings;
    settings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::GHOST_PARTICLES);
    /*settings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::BLOCK);
    settings.set(GlobalSettingsIds::DOMAIN_SIZE, Vector(1._f, 1._f, 20.05_f));*/
    settings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::CYLINDER);
    settings.set(GlobalSettingsIds::DOMAIN_HEIGHT, 20._f);
    settings.set(GlobalSettingsIds::DOMAIN_RADIUS, 0.5_f);
    settings.set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL);
    settings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false);
    settings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f);
    settings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-4_f);
    Problem p(settings, std::make_shared<Storage>());
    InitialConditions conds(p.storage, settings);

    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
    bodySettings.set(BodySettingsIds::DENSITY, 1000._f);
    bodySettings.set(BodySettingsIds::DENSITY_MIN, 1._f);
    bodySettings.set(BodySettingsIds::DENSITY_RANGE, Range(10._f, INFTY));
    const Float u0 = 1.e4_f;
    bodySettings.set(BodySettingsIds::ENERGY, u0);
    bodySettings.set(BodySettingsIds::ENERGY_RANGE, Range(10._f, INFTY));
    bodySettings.set(BodySettingsIds::ENERGY_MIN, 1._f);
    // conds.addBody(BlockDomain(Vector(0._f), Vector(1._f, 1._f, 20._f)), bodySettings);
    conds.addBody(CylindricalDomain(Vector(0._f), 0.5_f, 20._f, true), bodySettings);
    p.output =
        std::make_unique<GnuplotOutput>("out_%d.txt", "wave", "wave.plt", GnuplotOutput::Options::SCIENTIFIC);

    StdOutLogger logger;

    const Float gamma = bodySettings.get<Float>(BodySettingsIds::ADIABATIC_INDEX);
    const Float cs = Sph::sqrt(gamma * (gamma - 1._f) * u0);
    logger.write("Sound speed = ", cs);
    // find equilibrium state - in 0.1s, soundwave should propagate at least 5 times across the cylinder
    p.run();
}
