/// \file RotatingRod.cpp
/// \brief Rotating rod test
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "Common.h"
#include "catch.hpp"

using namespace Sph;

class AngularMomentumLog : public PeriodicTrigger {
private:
    std::ofstream ofs;

public:
    AngularMomentumLog(const Float period)
        : PeriodicTrigger(period, 0._f)
        , ofs("rod/angmom.txt") {}

    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        const Vector L = TotalAngularMomentum().evaluate(storage);
        ofs << t << "  " << L[Y] << "\n";
        return nullptr;
    }
};

class RotatingRod : public IRun {
public:
    RotatingRod() {
        settings.set(RunSettingsId::RUN_NAME, std::string("Rotating Rod Problem"))
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-3_f)
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
            .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::TEXT_FILE)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 50._f)
            .set(RunSettingsId::RUN_OUTPUT_PATH, std::string("rod"))
            .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("rod_%d.txt"))
            .set(RunSettingsId::RUN_OUTPUT_QUANTITIES,
                OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY | OutputQuantityFlag::DENSITY |
                    OutputQuantityFlag::PRESSURE | OutputQuantityFlag::ENERGY |
                    OutputQuantityFlag::SMOOTHING_LENGTH)
            .set(RunSettingsId::RUN_END_TIME, 2500._f)
            .set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS)
            .set(RunSettingsId::SPH_SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
            .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
            .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
            .set(RunSettingsId::SPH_AV_BETA, 3._f)
            .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.4_f)
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
        BodySettings body;
        body.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::ELASTIC)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::PARTICLE_COUNT, 5000);

        InitialConditions ic(settings);
        const Float height = 100.e3_f;
        const Float radius = 15e3_f;
        BodyView view =
            ic.addMonolithicBody(*storage, CylindricalDomain(Vector(0._f), radius, height, true), body);
        std::cout << "Created " << storage->getParticleCnt() << " particles" << std::endl;

        const Float spinRate = 2._f * PI / (3600._f * 1._f); // 1h
        view.addRotation(Vector(0._f, spinRate, 0._f), Vector(0._f));

        logWriter = makeAuto<NullLogWriter>();

        /// \todo change to logfile
        triggers.pushBack(makeAuto<ProgressLog>(25._f));

        triggers.pushBack(makeAuto<AngularMomentumLog>(1._f));
    }

protected:
    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        output->dump(storage, stats);
    }
};

TEST_CASE("Rotating rod", "[rod]") {
    RotatingRod run;
    Storage storage;
    run.run(storage);
}
