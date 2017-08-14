#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "run/Run.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "timestepping/TimeStepping.h"

using namespace Sph;

/*
TEST_CASE("StressForce Soundwave", "[stressforce]") {
    RunSettings settings;
    settings.set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::GHOST_PARTICLES);

settings.set(RunSettingsId::DOMAIN_TYPE, DomainEnum::CYLINDER);
settings.set(RunSettingsId::DOMAIN_HEIGHT, 20._f);
settings.set(RunSettingsId::DOMAIN_RADIUS, 0.5_f);
settings.set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL);
settings.set(RunSettingsId::MODEL_FORCE_DIV_S, false);
settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f);
settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e-4_f);
Problem p(settings, makeShared<Storage>());
InitialConditions conds(p.storage, settings);

BodySettings bodySettings;
bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10000);
bodySettings.set(BodySettingsId::DENSITY, 1000._f);
bodySettings.set(BodySettingsId::DENSITY_MIN, 1._f);
bodySettings.set(BodySettingsId::DENSITY_RANGE, Range(10._f, INFTY));
const Float u0 = 1.e4_f;
bodySettings.set(BodySettingsId::ENERGY, u0);
bodySettings.set(BodySettingsId::ENERGY_RANGE, Range(10._f, INFTY));
bodySettings.set(BodySettingsId::ENERGY_MIN, 1._f);
// conds.addBody(BlockDomain(Vector(0._f), Vector(1._f, 1._f, 20._f)), bodySettings);
conds.addBody(CylindricalDomain(Vector(0._f), 0.5_f, 20._f, true), bodySettings);
p.output =
    makeAuto<GnuplotOutput>("out_%d.txt", "wave", "wave.plt", GnuplotOutput::Options::SCIENTIFIC);

StdOutLogger logger;

const Float gamma = bodySettings.get<Float>(BodySettingsId::ADIABATIC_INDEX);
const Float cs = Sph::sqrt(gamma * (gamma - 1._f) * u0);
logger.write("Sound speed = ", cs);
// find equilibrium state - in 0.1s, soundwave should propagate at least 5 times across the cylinder
p.run();
}
*/
