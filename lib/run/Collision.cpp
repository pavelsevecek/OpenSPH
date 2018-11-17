#include "run/Collision.h"
#include "gravity/NBodySolver.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "io/Output.h"
#include "sph/Diagnostics.h"
#include "sph/solvers/StabilizationSolver.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

static RunSettings getSharedSettings(const Presets::CollisionParams& params, const std::string& fileMask) {
    // for 100km body, run 1000 s !
    // for 1000km body, run 10000s
    const Float runTime = 50._f * params.targetRadius / 1000;
    RunSettings settings;
    settings.set(RunSettingsId::RUN_NAME, std::string("Impact"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, runTime))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, runTime / 2000._f)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, params.outputPath.native())
        .set(RunSettingsId::RUN_OUTPUT_NAME, fileMask)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS | ForceEnum::GRAVITY)
        .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 10._f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::SPH_STABILIZATION_DAMPING, 0.4_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 1000)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::RUN_DIAGNOSTICS_INTERVAL, 1._f);
    return settings;
}

class EnergyLog : public ILogFile {
public:
    using ILogFile::ILogFile;

private:
    virtual void write(const Storage& storage, const Statistics& stats) override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float e = TotalEnergy().evaluate(storage);
        logger->write(t, "   ", e);
    }
};

/// ----------------------------------------------------------------------------------------------------------
/// Stabilization
/// ----------------------------------------------------------------------------------------------------------

StabilizationRunPhase::StabilizationRunPhase(const Presets::CollisionParams params)
    : params(params) {
    const Path stabPath = params.outputPath / Path("stabilization.sph");
    bool settingsLoaded;
    if (false && FileSystem::pathExists(stabPath)) {
        settings.loadFromFile(stabPath);
        settingsLoaded = true;
    } else {
        settings = getSharedSettings(params, "stab_%d.ssf");
        const Float runTime = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size();
        settings.set(RunSettingsId::RUN_NAME, std::string("Stabilization"))
            .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 0.1_f * runTime))
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.2_f * runTime / 20._f)
            .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f);
        //    .set(RunSettingsId::RUN_OUTPUT_TYPE, OutputEnum::NONE);
        settings.saveToFile(stabPath);
        settingsLoaded = false;
    }

    logger = Factory::getLogger(settings);

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
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 4.e6_f)
            .set(BodySettingsId::ENERGY_MIN, 10._f)
            .set(BodySettingsId::DAMAGE_MIN, 0.25_f)
            .set(BodySettingsId::PARTICLE_COUNT, 250000);
        body.saveToFile(matPath);
        logger->write("No material settings found, defaults saved to file '", matPath.native(), "'");
    }

    solver = makeAuto<StabilizationSolver>(*scheduler, settings);

    // override collision params with value loaded from settings
    params.targetParticleCnt = body.get<int>(BodySettingsId::PARTICLE_COUNT);
    params.body = body;
    data = makeShared<Presets::Collision>(*scheduler, settings, params);
    storage = makeShared<Storage>();

    data->addTarget(*storage);
    logger->write("Created target with ", storage->getParticleCnt(), " particles");

    logger->write(
        "Running STABILIZATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    output = Factory::getOutput(settings);

    // add printing of run progress
    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));

    const Float runTime = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size();
    SharedPtr<ILogger> energyLogger = makeShared<FileLogger>(params.outputPath / Path("stab_energy.txt"));
    AutoPtr<EnergyLog> energyFile = makeAuto<EnergyLog>(energyLogger, runTime / 50._f);
    triggers.pushBack(std::move(energyFile));

    diagnostics.push(makeAuto<CourantInstabilityDiagnostic>(20._f));
}

void StabilizationRunPhase::handoff(Storage&& input) {
    storage = makeShared<Storage>();
    storage->merge(std::move(input));
    solver = makeAuto<StabilizationSolver>(*scheduler, settings);

    IMaterial& material = storage->getMaterial(0);
    solver->create(*storage, material);
    MaterialInitialContext context(settings);
    material.create(*storage, context);

    logger = Factory::getLogger(settings);
    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));

    data = makeShared<Presets::Collision>(*scheduler, settings, params);

    // we need to create a "dummy" target, otherwise impactor would be created with flag 0
    /// \todo this is really dumb
    Storage dummy;
    data->addTarget(dummy);

    diagnostics.push(makeAuto<CourantInstabilityDiagnostic>(20._f));
}

AutoPtr<IRunPhase> StabilizationRunPhase::getNextPhase() const {
    return makeAuto<FragmentationRunPhase>(params, data);
}

/// ----------------------------------------------------------------------------------------------------------
/// Fragmentation
/// ----------------------------------------------------------------------------------------------------------

FragmentationRunPhase::FragmentationRunPhase(Presets::CollisionParams params,
    SharedPtr<Presets::Collision> data)
    : params(params)
    , data(data) {

    const Path fragPath = params.outputPath / Path("fragmentation.sph");
    bool settingsLoaded;
    if (false && FileSystem::pathExists(fragPath)) {
        settings.loadFromFile(fragPath);
        settingsLoaded = true;
    } else {
        settings = getSharedSettings(params, "frag_%d.ssf");
        settings.set(RunSettingsId::RUN_NAME, std::string("Fragmentation"))
            .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1000._f)
            .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
            .set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS)
            .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ENERGY_CONSERVING_SOLVER);
        settings.saveToFile(fragPath);
        settingsLoaded = false;
    }

    logger = Factory::getLogger(settings);

    if (settingsLoaded) {
        logger->write("Loaded fragmentation settings from file '", fragPath.native(), "'");
    } else {
        logger->write("No fragmentation settings found, defaults saved to file '", fragPath.native(), "'");
    }
}

AutoPtr<IRunPhase> FragmentationRunPhase::getNextPhase() const {
    return makeAuto<ReaccumulationRunPhase>(params.outputPath);
}

void FragmentationRunPhase::handoff(Storage&& input) {
    storage = makeShared<Storage>(std::move(input));
    solver = Factory::getSolver(*scheduler, settings);
    const Size targetParticleCnt = storage->getParticleCnt();
    data->addImpactor(*storage);

    logger = Factory::getLogger(settings);
    output = Factory::getOutput(settings);

    logger->write("Created impactor with ", storage->getParticleCnt() - targetParticleCnt, " particles");

    logger->write(
        "Running FRAGMENTATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));

    diagnostics.push(makeAuto<CourantInstabilityDiagnostic>(0.5_f));
    diagnostics.push(makeAuto<OvercoolingDiagnostic>());
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

/// ----------------------------------------------------------------------------------------------------------
/// Reaccumulation
/// ----------------------------------------------------------------------------------------------------------

ReaccumulationRunPhase::ReaccumulationRunPhase(const Path& outputPath) {
    settings.set(RunSettingsId::RUN_NAME, std::string("Reaccumulation"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e6_f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e4_f)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, outputPath.native())
        .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("reacc_%d.ssf"))
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::POINT_PARTICLES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::PASS_OR_MERGE)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.5_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
        .set(RunSettingsId::COLLISION_ALLOWED_OVERLAP, 0.1_f)
        .set(RunSettingsId::COLLISION_MERGING_LIMIT, 1._f)
        .set(RunSettingsId::NBODY_INERTIA_TENSOR, false)
        .set(RunSettingsId::NBODY_MAX_ROTATION_ANGLE, 0.01_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
}


AutoPtr<IRunPhase> ReaccumulationRunPhase::getNextPhase() const {
    return nullptr;
}

void ReaccumulationRunPhase::handoff(Storage&& input) {
    solver = makeAuto<NBodySolver>(*scheduler, settings);

    // we don't need any material, so just pass some dummy
    storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));

    // clone required quantities
    storage->insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, input.getValue<Vector>(QuantityId::POSITION).clone());
    storage->getDt<Vector>(QuantityId::POSITION) = input.getDt<Vector>(QuantityId::POSITION).clone();
    storage->insert<Float>(
        QuantityId::MASS, OrderEnum::ZERO, input.getValue<Float>(QuantityId::MASS).clone());

    // radii handoff
    ArrayView<const Float> m = input.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho = input.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    ASSERT(r.size() == rho.size());
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = cbrt(3._f * m[i] / (4._f * PI * rho[i]));
    }

    // remove all sublimated particles
    Array<Size> toRemove;
    ArrayView<const Float> u = input.getValue<Float>(QuantityId::ENERGY);
    for (Size matId = 0; matId < input.getMaterialCnt(); ++matId) {
        MaterialView mat = input.getMaterial(matId);
        const Float u_max = mat->getParam<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
        for (Size i : mat.sequence()) {
            if (u[i] > u_max) {
                toRemove.push(i);
            }
        }
    }
    storage->remove(toRemove);

    // move to COM system
    ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
    moveToCenterOfMassSystem(m, v);
    moveToCenterOfMassSystem(m, r);

    // create additional quantities (angular velocity, ...)
    solver->create(*storage, storage->getMaterial(0));
    ASSERT(storage->isValid());

    logger = Factory::getLogger(settings);
    output = Factory::getOutput(settings);

    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings), settings));
}

void ReaccumulationRunPhase::tearDown(const Statistics& UNUSED(stats)) {}

NAMESPACE_SPH_END
