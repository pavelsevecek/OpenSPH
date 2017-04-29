#include "geometry/Domain.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Eos.h"
#include "run/Run.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "timestepping/TimeStepping.h"

using namespace Sph;


class Run : public Abstract::Run {
public:
    Run() {
        settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-6_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e-1_f)
            .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
            .set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 1._f));
    }

    virtual std::shared_ptr<Storage> setUp() override {
        const std::string outputName = settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME);
        const std::string runName = settings.get<std::string>(RunSettingsId::RUN_NAME);
        output = std::make_unique<TextOutput>(outputName, runName, TextOutput::Options::SCIENTIFIC);
        output->add(std::make_unique<ParticleNumberColumn>());
        output->add(std::make_unique<ValueColumn<Vector>>(QuantityId::POSITIONS));

        storage = std::make_shared<Storage>();

        BodySettings bodySettings;
        bodySettings.set(BodySettingsId::ENERGY, 1.e2_f);
        bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10000);
        bodySettings.set(BodySettingsId::EOS, EosEnum::TILLOTSON);
        InitialConditions conds(*storage, settings);

        SphericalDomain domain1(Vector(0._f), 5e2_f); // D = 1km
        conds.addBody(domain1, bodySettings);

        SphericalDomain domain2(Vector(6.e2_f, 1.35e2_f, 0._f), 20._f);
        bodySettings.set(BodySettingsId::PARTICLE_COUNT, 100);
        conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f));

        return storage;
    }

protected:
    virtual void tearDown() override {
        StdOutLogger logger;
        Profiler::getInstance()->printStatistics(logger);
    }
};


int main() {
    Run run;
    run.setUp();
    run.run();
    return 0;
}
