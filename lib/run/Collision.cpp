#include "run/Collision.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "io/Output.h"
#include "sph/solvers/StabilizationSolver.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

static RunSettings getSharedSettings(const Presets::CollisionParams& params, const std::string& fileMask) {
    // for 100km body, run 100 s
    const Float runTime = 2._f * params.targetRadius / 1000;
    RunSettings settings;
    settings.set(RunSettingsId::RUN_NAME, std::string("Impact"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1._f)
        .set(RunSettingsId::TIMESTEPPING_MAX_CHANGE, 0.1_f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.25_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, runTime))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, runTime / 100._f)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, OutputEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, params.outputPath.native())
        .set(RunSettingsId::RUN_OUTPUT_NAME, fileMask)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS | ForceEnum::GRAVITY)
        .set(RunSettingsId::SPH_FORMULATION, FormulationEnum::STANDARD)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true);
    return settings;
}

StabilizationRunPhase::StabilizationRunPhase(const Presets::CollisionParams params)
    : params(params) {
    const Path stabPath = params.outputPath / Path("stabilization.sph");
    bool settingsLoaded;
    if (FileSystem::pathExists(stabPath)) {
        settings.loadFromFile(stabPath);
        settingsLoaded = true;
    } else {
        settings = getSharedSettings(params, "stab_%d.ssf");
        settings.set(RunSettingsId::RUN_NAME, std::string("Stabilization"))
            .set(RunSettingsId::RUN_OUTPUT_TYPE, OutputEnum::NONE);
        settings.saveToFile(stabPath);
        settingsLoaded = false;
    }

    logger = Factory::getLogger(settings);
    output = Factory::getOutput(settings);

    if (settingsLoaded) {
        logger->write("Loaded stabilization settings from file '", stabPath.native(), "'");
    } else {
        logger->write("No stabilization settings found, defaults saved to file '", stabPath.native(), "'");
    }
}

void StabilizationRunPhase::setUp() {
    logger = Factory::getLogger(settings);

    BodySettings body;
    const Path matPath = params.outputPath / Path("material.sph");
    if (FileSystem::pathExists(matPath)) {
        body.loadFromFile(matPath);
        logger->write("Loaded material settings from file '", matPath.native(), "'");
    } else {
        body.set(BodySettingsId::ENERGY, 0._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 5.e5_f)
            .set(BodySettingsId::ENERGY_MIN, 1._f)
            .set(BodySettingsId::DAMAGE_MIN, 0.2_f);
        body.saveToFile(matPath);
        logger->write("No material settings found, defaults saved to file '", matPath.native(), "'");
    }

    solver = makeAuto<StabilizationSolver>(settings);
    data = makeShared<Presets::Collision>(settings, body, params);
    storage = makeShared<Storage>();

    data->addTarget(*storage);
    logger->write("Created target with ", storage->getParticleCnt(), " particles");

    logger->write(
        "Running STABILIZATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    // add printing of run progress
    triggers.pushBack(makeAuto<CommonStatsLog>(logger));
}

AutoPtr<IRunPhase> StabilizationRunPhase::getNextPhase() const {
    return makeAuto<FragmentationRunPhase>(params, data);
}


FragmentationRunPhase::FragmentationRunPhase(Presets::CollisionParams params,
    SharedPtr<Presets::Collision> data)
    : params(params)
    , data(data) {

    const Path fragPath = params.outputPath / Path("fragmentation.sph");
    bool settingsLoaded;
    if (FileSystem::pathExists(fragPath)) {
        settings.loadFromFile(fragPath);
        settingsLoaded = true;
    } else {
        settings = getSharedSettings(params, "frag_%d.ssf");
        settings.set(RunSettingsId::RUN_NAME, std::string("Fragmentation"))
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1000._f);
        settings.saveToFile(fragPath);
        settingsLoaded = false;
    }

    logger = Factory::getLogger(settings);
    output = Factory::getOutput(settings);

    if (settingsLoaded) {
        logger->write("Loaded fragmentation settings from file '", fragPath.native(), "'");
    } else {
        logger->write("No fragmentation settings found, defaults saved to file '", fragPath.native(), "'");
    }
}

void FragmentationRunPhase::handoff(Storage&& input) {
    storage = makeShared<Storage>(std::move(input));
    solver = Factory::getSolver(settings);
    const Size targetParticleCnt = storage->getParticleCnt();
    data->addImpactor(*storage);
    logger->write("Created impactor with ", storage->getParticleCnt() - targetParticleCnt, " particles");

    logger->write(
        "Running FRAGMENTATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    triggers.pushBack(makeAuto<CommonStatsLog>(logger));
}

void FragmentationRunPhase::tearDown(const Statistics& stats) {
    // save the final result in multiple formats
    PkdgravParams pkd;
    pkd.omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
    PkdgravOutput pkdgravOutput(params.outputPath / Path("pkdgrav/pkdgrav.out"), std::move(pkd));
    pkdgravOutput.dump(*storage, stats);

    Flags<OutputQuantityFlag> quantities = OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY |
                                           OutputQuantityFlag::DENSITY | OutputQuantityFlag::PRESSURE |
                                           OutputQuantityFlag::DEVIATORIC_STRESS | OutputQuantityFlag::MASS |
                                           OutputQuantityFlag::ENERGY | OutputQuantityFlag::SMOOTHING_LENGTH |
                                           OutputQuantityFlag::DAMAGE | OutputQuantityFlag::INDEX;
    TextOutput textOutput(params.outputPath / Path("output.txt"), "impact", quantities);
    textOutput.dump(*storage, stats);

    BinaryOutput binaryOutput(params.outputPath / Path("output.ssf"));
    binaryOutput.dump(*storage, stats);
}


NAMESPACE_SPH_END
