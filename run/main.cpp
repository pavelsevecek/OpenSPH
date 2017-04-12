#include "geometry/Domain.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Eos.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "timestepping/TimeStepping.h"

using namespace Sph;


class Problem : public Abstract::Problem {
public:
    Problem() {
        this->settings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL)
            .set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f)
            .set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
            .set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true)
            .set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL)
            .set(GlobalSettingsIds::RUN_TIME_RANGE, Range(0._f, 1._f));
        /*globalSettings.set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP);
        globalSettings.set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);*/
    }

protected:
    virtual void setUp() override {
        this->output =
            std::make_unique<TextOutput>(this->settings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME),
                this->settings.get<std::string>(GlobalSettingsIds::RUN_NAME),
                TextOutput::Options::SCIENTIFIC);
        this->output->add(std::make_unique<ValueColumn<Vector>>(QuantityIds::POSITIONS));

        this->storage = std::make_shared<Storage>();

        BodySettings bodySettings;
        bodySettings.set(BodySettingsIds::ENERGY, 1.e2_f);
        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
        bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
        InitialConditions conds(this->storage, this->settings);

        SphericalDomain domain1(Vector(0._f), 5e2_f); // D = 1km
        conds.addBody(domain1, bodySettings);

        SphericalDomain domain2(Vector(6.e2_f, 1.35e2_f, 0._f), 20._f);
        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
        conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f));
    }

    virtual void tearDown() override {
        StdOutLogger logger;
        Profiler::getInstance()->printStatistics(logger);
    }
};


int main() {
    Problem problem;
    problem.run();
    return 0;
}
