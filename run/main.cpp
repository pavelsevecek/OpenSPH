#include "geometry/Domain.h"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Factory.h"
#include "system/Io.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "system/Profiler.h"

using namespace Sph;

int main() {
    GlobalSettings globalSettings;
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true);
    globalSettings.set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL);
    globalSettings.set(GlobalSettingsIds::RUN_TIME_RANGE, Range(0._f, 1._f));
    /*globalSettings.set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP);
    globalSettings.set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);*/
    Problem* p = new Problem(globalSettings, std::make_shared<Storage>());
    p->output =
        std::make_unique<TextOutput>(globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME),
            globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME));

    BodySettings bodySettings;
    bodySettings.set(BodySettingsIds::ENERGY, 1.e2_f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
    InitialConditions conds(p->storage, globalSettings);

    SphericalDomain domain1(Vector(0._f), 5e2_f); // D = 1km
    conds.addBody(domain1, bodySettings);

    /*SphericalDomain domain2(Vector(6.e2_f, 1.35e2_f, 0._f), 20._f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f));*/
    // p->run();

    StdOutLogger logger;
    Profiler::getInstance()->printStatistics(logger);

    if (!sendMail("sevecek@sirrah.troja.mff.cuni.cz",
            "pavel.sevecek@gmail.com",
            "run ended",
            "run has ended, yo")) {
        logger.write("Cannot send mail");
    }
    return 0;
}
