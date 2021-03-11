/// \file Galaxy.cpp
/// \brief Initial conditions and the evolution of a galaxy
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "gravity/Galaxy.h"
#include "Common.h"
#include "catch.hpp"

using namespace Sph;

class GalaxyRun : public IRun {
public:
    GalaxyRun() {
        settings.set(RunSettingsId::RUN_NAME, std::string("Galaxy Problem"))
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-3_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 20._f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string("galaxy"))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("galaxy_%d.ssf"))
            .set(RunSettingsId::RUN_END_TIME, 10._f)
            .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES)
            .set(RunSettingsId::GRAVITY_CONSTANT, 1._f)
            .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL)
            .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::ELASTIC_BOUNCE)
            .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 1._f)
            .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
            .set(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR, 1._f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);

        scheduler = Factory::getScheduler(settings);
    }

    virtual void setUp(SharedPtr<Storage> storage) override {

        GalaxySettings galaxy;
        galaxy.set(GalaxySettingsId::PARTICLE_RADIUS, 0.001_f);

        *storage = Galaxy::generateIc(settings, galaxy);

        solver = makeAuto<HardSphereSolver>(*scheduler, settings);

        logWriter = makeAuto<NullLogWriter>();

        /// \todo change to logfile
        triggers.pushBack(makeAuto<ProgressLog>(0.5_f));
    }

protected:
    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        output->dump(storage, stats);
    }
};

TEST_CASE("Galaxy", "[galaxy]") {
    Array<Path> filesToCheck = { Path("galaxy/galaxy_0000.ssf"), Path("galaxy/galaxy_0001.ssf") };

    for (Path file : filesToCheck) {
        FileSystem::removePath(file);
    }

    measureRun(Path("galaxy/stats"), [] {
        GalaxyRun run;
        Storage storage;
        run.run(storage);
    });

    for (Path file : filesToCheck) {
        REQUIRE(areFilesApproxEqual(file, REFERENCE_DIR / file.fileName()) == SUCCESS);
    }
}
