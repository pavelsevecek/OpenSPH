/// \file Collision.cpp
/// \brief Test of an asteroid collision
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "Common.h"
#include "catch.hpp"

using namespace Sph;

class Collision : public IRun {
public:
    Collision() {
        settings.set(RunSettingsId::RUN_NAME, std::string("Asteroid Collision Problem"))
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-8_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 50._f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string("collision"))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("collision_%d.ssf"))
            .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 500._f))
            .set(RunSettingsId::SPH_SOLVER_FORCES,
                ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS | ForceEnum::GRAVITY)
            .set(RunSettingsId::SPH_SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
            .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
            .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
            .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
            .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
            .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
            .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
            .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
            .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 1._f)
            .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
            .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));
    }

    virtual void setUp() override {
        storage = makeShared<Storage>();

        CollisionParams params;
        params.targetBody.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL);
        params.impactorBody = params.targetBody;

        params.geometry.set(CollisionGeometrySettingsId::TARGET_RADIUS, 10.e3_f)
            .set(CollisionGeometrySettingsId::IMPACTOR_RADIUS, 1.e3_f)
            .set(CollisionGeometrySettingsId::TARGET_SPIN_RATE, 24._f / 4._f) // 4h
            .set(CollisionGeometrySettingsId::IMPACT_ANGLE, 45._f)
            .set(CollisionGeometrySettingsId::IMPACT_SPEED, 5.e3_f)
            .set(CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT, 10000);
        params.outputPath = Path("collision");

        CollisionInitialConditions collision(*scheduler, settings, params);
        collision.addTarget(*storage);
        collision.addImpactor(*storage);

        logWriter = makeAuto<NullLogWriter>();

        /// \todo change to logfile
        triggers.pushBack(makeAuto<ProgressLog>(25._f));
    }

protected:
    virtual void tearDown(const Statistics& UNUSED(stats)) override {}
};

TEST_CASE("Collision", "[collision]") {
    Array<Path> filesToCheck = { Path("collision/collision_0004.ssf"), Path("collision/collision_0009.ssf") };

    for (Path file : filesToCheck) {
        FileSystem::removePath(file);
    }

    // parameters lose some precision after writing to .sph (ascii) file, so we never want to load them if we
    // wish to get *precisely* the same result (to the bit).
    FileSystem::removePath(Path("collision/target.sph"));
    FileSystem::removePath(Path("collision/impactor.sph"));

    Collision run;
    run.setUp();
    run.run();

    for (Path file : filesToCheck) {
        REQUIRE(areFilesApproxEqual(file, REFERENCE_DIR / file.fileName()) == SUCCESS);
    }
}
