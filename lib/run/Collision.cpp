#include "run/Collision.h"
#include "gravity/NBodySolver.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "io/Output.h"
#include "objects/Exceptions.h"
#include "quantities/Quantity.h"
#include "sph/Diagnostics.h"
#include "sph/solvers/StabilizationSolver.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/// \brief Returns settings shared for stabilization and fragmentation phase.
static RunSettings getSphSettings(const Interval timeRange,
    const Size dumpCnt,
    const Path& outputPath,
    const std::string& fileMask) {
    RunSettings settings;
    settings.set(RunSettingsId::RUN_NAME, std::string("Impact"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
        .set(RunSettingsId::RUN_TIME_RANGE, timeRange)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, timeRange.size() / dumpCnt)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, outputPath.native())
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
        .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 5._f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::SPH_STABILIZATION_DAMPING, 0.1_f)
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

StabilizationRunPhase::StabilizationRunPhase(const Presets::CollisionParams collisionParams,
    const PhaseParams phaseParams)
    : collisionParams(collisionParams)
    , phaseParams(phaseParams) {
    this->create(phaseParams);

    const Path collisionPath = collisionParams.outputPath / Path("collision.sph");
    if (FileSystem::pathExists(collisionPath)) {
        this->collisionParams.loadFromFile(collisionPath);
    } else {
        this->collisionParams.saveToFile(collisionPath);
    }
}

StabilizationRunPhase::StabilizationRunPhase(const Path& resumePath, const PhaseParams phaseParams)
    : phaseParams(phaseParams)
    , resumePath(resumePath) {
    this->create(phaseParams);
}

void StabilizationRunPhase::create(const PhaseParams phaseParams) {
    const Path stabPath = phaseParams.outputPath / Path("stabilization.sph");
    bool settingsLoaded;
    if (FileSystem::pathExists(stabPath)) {
        settings.loadFromFile(stabPath);
        settingsLoaded = true;
    } else {
        settings = getSphSettings(phaseParams.stab.range, 1, phaseParams.outputPath, "stab_%d.ssf");
        settings.set(RunSettingsId::RUN_NAME, std::string("Stabilization"))
            .set(RunSettingsId::RUN_TYPE, RunTypeEnum::STABILIZATION)
            .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::NONE);
        settings.addEntries(phaseParams.stab.overrides);
        settings.saveToFile(stabPath);
        settingsLoaded = false;
    }

    scheduler = Factory::getScheduler(settings);
    logger = Factory::getLogger(settings);

    if (settingsLoaded) {
        logger->write("Loaded stabilization settings from file '", stabPath.native(), "'");
    } else {
        logger->write("No stabilization settings found, defaults saved to file '", stabPath.native(), "'");
    }
}

void StabilizationRunPhase::setUp() {
    logger = Factory::getLogger(settings);
    storage = makeShared<Storage>();
    solver = makeAuto<StabilizationSolver>(*scheduler, settings);

    BodySettings body;
    body.set(BodySettingsId::ENERGY, 1.e3_f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 4.e6_f)
        .set(BodySettingsId::ENERGY_MIN, 10._f)
        .set(BodySettingsId::DAMAGE_MIN, 0.25_f)
        .set(BodySettingsId::PARTICLE_COUNT, int(collisionParams.targetParticleCnt));
    body.addEntries(collisionParams.body);
    collisionParams.body = std::move(body);

    collision = makeShared<Presets::Collision>(*scheduler, settings, collisionParams);

    // create the target even when continuing from file, so that we can than add an impactor in fragmentation
    collision->addTarget(*storage);

    if (!resumePath.empty()) {
        BinaryInput input;
        Statistics stats;
        const Outcome result = input.load(resumePath, *storage, stats);
        if (!result) {
            throw InvalidSetup("Cannot open or parse file " + resumePath.native() + "\n" + result.error());
        } else {
            logger->write("Loaded state file containing ", storage->getParticleCnt(), " particles.");
        }
    } else {
        logger->write("Created target with ", storage->getParticleCnt(), " particles");
    }

    logger->write(
        "Running STABILIZATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    // add printing of run progress
    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));

    const Float runTime = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size();
    SharedPtr<ILogger> energyLogger =
        makeShared<FileLogger>(collisionParams.outputPath / Path("stab_energy.txt"));
    AutoPtr<EnergyLog> energyFile = makeAuto<EnergyLog>(energyLogger, runTime / 50._f);
    triggers.pushBack(std::move(energyFile));
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

    collision = makeShared<Presets::Collision>(*scheduler, settings, collisionParams);

    // we need to create a "dummy" target, otherwise impactor would be created with flag 0
    /// \todo this is really dumb
    Storage dummy;
    collision->addTarget(dummy);

    diagnostics.push(makeAuto<CourantInstabilityDiagnostic>(20._f));
}

AutoPtr<IRunPhase> StabilizationRunPhase::getNextPhase() const {
    return makeAuto<FragmentationRunPhase>(*this);
}

/// ----------------------------------------------------------------------------------------------------------
/// Fragmentation
/// ----------------------------------------------------------------------------------------------------------

FragmentationRunPhase::FragmentationRunPhase(const StabilizationRunPhase& stabilization)
    : collisionParams(stabilization.collisionParams)
    , phaseParams(stabilization.phaseParams)
    , collision(stabilization.collision) {

    this->create(phaseParams);
}

FragmentationRunPhase::FragmentationRunPhase(const Path& resumePath, const PhaseParams phaseParams)
    : phaseParams(phaseParams)
    , resumePath(resumePath) {
    this->create(phaseParams);

    if (Optional<Size> firstIdx = OutputFile::getDumpIdx(resumePath)) {
        settings.set(RunSettingsId::RUN_OUTPUT_FIRST_INDEX, int(firstIdx.value() + 1));
    }
}

void FragmentationRunPhase::create(const PhaseParams phaseParams) {
    const Path fragPath = phaseParams.outputPath / Path("fragmentation.sph");
    bool settingsLoaded;
    if (FileSystem::pathExists(fragPath)) {
        settings.loadFromFile(fragPath);
        settingsLoaded = true;
    } else {
        settings = getSphSettings(
            phaseParams.frag.range, phaseParams.frag.dumpCnt, phaseParams.outputPath, "frag_%d.ssf");
        settings.set(RunSettingsId::RUN_NAME, std::string("Fragmentation"))
            .set(RunSettingsId::RUN_TYPE, RunTypeEnum::SPH);
        settings.addEntries(phaseParams.frag.overrides);
        settings.saveToFile(fragPath);
        settingsLoaded = false;
    }

    scheduler = Factory::getScheduler(settings);
    logger = Factory::getLogger(settings);

    if (settingsLoaded) {
        logger->write("Loaded fragmentation settings from file '", fragPath.native(), "'");
    } else {
        logger->write("No fragmentation settings found, defaults saved to file '", fragPath.native(), "'");
    }
}

void FragmentationRunPhase::setUp() {
    ASSERT(!resumePath.empty());

    storage = makeShared<Storage>();
    solver = Factory::getSolver(*scheduler, settings);

    BinaryInput input;
    Statistics stats;
    const Outcome result = input.load(resumePath, *storage, stats);
    if (!result) {
        throw InvalidSetup("Cannot open or parse file " + resumePath.native() + "\n" + result.error());
    } else {
        logger = Factory::getLogger(settings);
        logger->write("Loaded state file containing ", storage->getParticleCnt(), " particles.");
    }

    logger->write(
        "Running FRAGMENTATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));
}

AutoPtr<IRunPhase> FragmentationRunPhase::getNextPhase() const {
    return makeAuto<ReaccumulationRunPhase>(*this);
}

void FragmentationRunPhase::handoff(Storage&& input) {
    storage = makeShared<Storage>(std::move(input));
    solver = Factory::getSolver(*scheduler, settings);

    const Size targetParticleCnt = storage->getParticleCnt();
    collision->addImpactor(*storage);

    logger = Factory::getLogger(settings);

    logger->write("Created impactor with ", storage->getParticleCnt() - targetParticleCnt, " particles");
    logger->write(
        "Running FRAGMENTATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));
}

void FragmentationRunPhase::tearDown(const Statistics& stats) {
    Flags<OutputQuantityFlag> quantities = OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY |
                                           OutputQuantityFlag::DENSITY | OutputQuantityFlag::PRESSURE |
                                           OutputQuantityFlag::DEVIATORIC_STRESS | OutputQuantityFlag::MASS |
                                           OutputQuantityFlag::ENERGY | OutputQuantityFlag::SMOOTHING_LENGTH |
                                           OutputQuantityFlag::DAMAGE | OutputQuantityFlag::INDEX;
    TextOutput textOutput(phaseParams.outputPath / Path("frag_final.txt"), "impact", quantities);
    textOutput.dump(*storage, stats);

    BinaryOutput binaryOutput(phaseParams.outputPath / Path("frag_final.ssf"), RunTypeEnum::SPH);
    binaryOutput.dump(*storage, stats);
}

/// ----------------------------------------------------------------------------------------------------------
/// Reaccumulation
/// ----------------------------------------------------------------------------------------------------------

static RunSettings getReaccSettings(const PhaseParams phaseParams) {
    RunSettings settings;
    settings.set(RunSettingsId::RUN_NAME, std::string("Reaccumulation"))
        .set(RunSettingsId::RUN_TYPE, RunTypeEnum::NBODY)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::RUN_TIME_RANGE, phaseParams.reacc.range)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, phaseParams.reacc.range.size() / phaseParams.reacc.dumpCnt)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, phaseParams.outputPath.native())
        .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("reacc_%d.ssf"))
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::PASS_OR_MERGE)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.5_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
        .set(RunSettingsId::COLLISION_ALLOWED_OVERLAP, 0.01_f)
        .set(RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT, 4._f)
        .set(RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT, 1._f)
        .set(RunSettingsId::NBODY_INERTIA_TENSOR, false)
        .set(RunSettingsId::NBODY_MAX_ROTATION_ANGLE, 0.01_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
    settings.addEntries(phaseParams.reacc.overrides);
    return settings;
}

ReaccumulationRunPhase::ReaccumulationRunPhase(const FragmentationRunPhase& fragmentation)
    : phaseParams(fragmentation.phaseParams) {
    this->create(phaseParams);
}

ReaccumulationRunPhase::ReaccumulationRunPhase(const Path& resumePath, const PhaseParams phaseParams)
    : phaseParams(phaseParams)
    , resumePath(resumePath) {
    this->create(phaseParams);

    if (Optional<Size> firstIdx = OutputFile::getDumpIdx(resumePath)) {
        settings.set(RunSettingsId::RUN_OUTPUT_FIRST_INDEX, int(firstIdx.value() + 1));
    }
}

void ReaccumulationRunPhase::create(const PhaseParams phaseParams) {
    const Path reaccPath = phaseParams.outputPath / Path("reaccumulation.sph");
    bool settingsLoaded;
    if (FileSystem::pathExists(reaccPath)) {
        settings.loadFromFile(reaccPath);
        settingsLoaded = true;
    } else {
        settings = getReaccSettings(phaseParams);
        settings.saveToFile(reaccPath);
        settingsLoaded = false;
    }

    scheduler = Factory::getScheduler(settings);
    logger = Factory::getLogger(settings);

    if (settingsLoaded) {
        logger->write("Loaded reaccumulation settings from file '", reaccPath.native(), "'");
    } else {
        logger->write("No reaccumulation settings found, defaults saved to file '", reaccPath.native(), "'");
    }
}

void ReaccumulationRunPhase::setUp() {
    ASSERT(!resumePath.empty());

    storage = makeShared<Storage>();
    solver = makeAuto<NBodySolver>(*scheduler, settings);

    BinaryInput input;
    Statistics stats;
    const Outcome result = input.load(resumePath, *storage, stats);
    if (!result) {
        throw InvalidSetup("Cannot open or parse file " + resumePath.native() + "\n" + result.error());
    } else {
        logger = Factory::getLogger(settings);
        logger->write("Loaded state file containing ", storage->getParticleCnt(), " particles.");
    }

    logger->write(
        "Running REACCUMULATION for ", settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE).size(), " s");

    triggers.pushBack(makeAuto<CommonStatsLog>(logger, settings));
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
    ArrayView<Vector> v, dummy;
    tie(r, v, dummy) = storage->getAll<Vector>(QuantityId::POSITION);
    m = input.getValue<Float>(QuantityId::MASS);
    moveToCenterOfMassSystem(m, v);
    moveToCenterOfMassSystem(m, r);

    // create additional quantities (angular velocity, ...)
    solver->create(*storage, storage->getMaterial(0));
    ASSERT(storage->isValid());

    logger = Factory::getLogger(settings);

    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings), settings));
}

AutoPtr<IRunPhase> ReaccumulationRunPhase::getNextPhase() const {
    return nullptr;
}

void ReaccumulationRunPhase::tearDown(const Statistics& stats) {
    BinaryOutput binaryOutput(phaseParams.outputPath / Path("reacc_final.ssf"), RunTypeEnum::NBODY);
    binaryOutput.dump(*storage, stats);
}

/// ----------------------------------------------------------------------------------------------------------
/// CollisionRun
/// ----------------------------------------------------------------------------------------------------------

CollisionRun::CollisionRun(const Presets::CollisionParams collisionParams,
    const PhaseParams phaseParams,
    SharedPtr<IRunCallbacks> runCallbacks) {
    first = makeShared<StabilizationRunPhase>(collisionParams, phaseParams);
    callbacks = runCallbacks;
}

CollisionRun::CollisionRun(const Path& path, PhaseParams phaseParams, SharedPtr<IRunCallbacks> runCallbacks) {
    BinaryInput input;
    Expected<BinaryInput::Info> info = input.getInfo(path);
    if (!info) {
        throw InvalidSetup("Cannot get header information from file " + path.native() + "\n" + info.error());
    }

    if (info->runType) {
        switch (info->runType.value()) {
        case RunTypeEnum::STABILIZATION:
            phaseParams.stab.range = Interval(info->runTime, phaseParams.stab.range.upper());
            phaseParams.stab.overrides.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, info->timeStep);
            first = makeShared<FragmentationRunPhase>(path, phaseParams);
            break;
        case RunTypeEnum::SPH:
            phaseParams.frag.range = Interval(info->runTime, phaseParams.frag.range.upper());
            phaseParams.frag.overrides.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, info->timeStep);
            first = makeShared<FragmentationRunPhase>(path, phaseParams);
            break;
        case RunTypeEnum::NBODY:
            phaseParams.reacc.range = Interval(info->runTime, phaseParams.reacc.range.upper());
            phaseParams.reacc.overrides.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, info->timeStep);
            first = makeShared<ReaccumulationRunPhase>(path, phaseParams);
            break;
        case RunTypeEnum::RUBBLE_PILE:
            throw InvalidSetup("Cannot resume rubble-pile simulation");
        }
    } else {
        throw InvalidSetup("Invalid file format, cannot determine run phase.");
    }

    callbacks = runCallbacks;
}

void CollisionRun::setOnNextPhase(Function<void(const IRunPhase&)> newOnPhasePhase) {
    onNextPhase = newOnPhasePhase;
}

NAMESPACE_SPH_END
