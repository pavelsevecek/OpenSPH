/// \file Collision.cpp
/// \brief Test of an asteroid collision
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

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
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 50._f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string("collision"))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("collision_%d.ssf"))
            .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 500._f))
            .set(RunSettingsId::SOLVER_FORCES,
                ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS | ForceEnum::GRAVITY)
            .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
            .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
            .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
            .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
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

        Presets::CollisionParams params;
        params.body.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::ENERGY_MIN, 100._f)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e6_f)
            .set(BodySettingsId::DAMAGE_MIN, 10._f);

        params.targetRadius = 10.e3_f;
        params.impactorRadius = 2.e3_f;
        params.targetRotation = 2._f * PI / (3600._f * 4._f);
        params.impactAngle = 45._f * DEG_TO_RAD;
        params.impactSpeed = 5.e3_f;
        params.targetParticleCnt = 10000;

        Presets::Collision collision(*scheduler, settings, params);
        collision.addTarget(*storage);
        collision.addImpactor(*storage);

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

    Collision run;
    run.setUp();
    run.run();

    for (Path file : filesToCheck) {
        REQUIRE(areFilesEqual(file, REFERENCE_DIR / file.fileName()));
    }
}
