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
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1000._f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string("collision"))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("collision_%d.ssf"))
            .set(RunSettingsId::RUN_END_TIME, 500._f)
            .set(RunSettingsId::SPH_SOLVER_FORCES,
                ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS | ForceEnum::SELF_GRAVITY)
            .set(RunSettingsId::SPH_SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
            .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
            .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
            .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
            .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
            .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
            .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
            .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
            .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 1._f)
            .set(RunSettingsId::FINDER_LEAF_SIZE, 20)
            .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));

        scheduler = Factory::getScheduler(settings);
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        BodySettings targetBody;
        targetBody.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::PARTICLE_COUNT, 10000);

        BodySettings impactorBody = targetBody;
        impactorBody.set(BodySettingsId::PARTICLE_COUNT, 100)
            .set(BodySettingsId::DAMAGE_MIN, LARGE)
            .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE);

        InitialConditions ic(settings);
        const Float targetRadius = 10.e3_f;
        BodyView targetView =
            ic.addMonolithicBody(*storage, SphericalDomain(Vector(0._f), targetRadius), targetBody);

        const Float spinRate = 2._f * PI / (3600._f * 4._f); // 4h
        targetView.addRotation(Vector(0._f, 0._f, spinRate), Vector(0._f));

        const Float impactAngle = 45._f * DEG_TO_RAD;
        const Float impactorRadius = 1.e3_f;
        const Vector impactorOrigin =
            (targetRadius + impactorRadius) * Vector(cos(impactAngle) + 0.05_f, sin(impactAngle), 0._f);
        BodyView impactorView =
            ic.addMonolithicBody(*storage, SphericalDomain(impactorOrigin, impactorRadius), impactorBody);
        impactorView.addVelocity(Vector(-5.e3_f, 0._f, 0._f));

        logWriter = makeAuto<NullLogWriter>();

        /// \todo change to logfile
        triggers.pushBack(makeAuto<ProgressLog>(25._f));
    }

protected:
    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        output->dump(storage, stats);
    }
};

TEST_CASE("Collision", "[collision]") {
    Array<Path> filesToCheck = { Path("collision/collision_0000.ssf"), Path("collision/collision_0001.ssf") };

    for (Path file : filesToCheck) {
        FileSystem::removePath(file);
    }

    measureRun(Path("collision/stats"), [] {
        Collision run;
        Storage storage;
        run.run(storage);
    });

    for (Path file : filesToCheck) {
        REQUIRE(areFilesApproxEqual(file, REFERENCE_DIR / file.fileName()) == SUCCESS);
    }
}
