#include "run/workers/SimulationJobs.h"
#include "gravity/AggregateSolver.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "run/IRun.h"
#include "run/SpecialEntries.h"
#include "sph/solvers/StabilizationSolver.h"

NAMESPACE_SPH_BEGIN

/// \todo generailize, add generic triggers to UI
class EnergyLogWriter : public ILogWriter {
public:
    using ILogWriter::ILogWriter;

private:
    virtual void write(const Storage& storage, const Statistics& stats) override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float e = TotalEnergy().evaluate(storage);
        logger->write(t, "   ", e);
    }
};

static std::string getIdentifier(const std::string& name) {
    std::string escaped = replaceAll(name, " ", "-");
    return lowercase(escaped);
}

// ----------------------------------------------------------------------------------------------------------
// SphWorker
// ----------------------------------------------------------------------------------------------------------

static RunSettings overrideSettings(const RunSettings& settings,
    const RunSettings& overrides,
    const bool isResumed) {
    RunSettings actual = settings;
    actual.addEntries(overrides);

    if (!isResumed) {
        // reset the (potentially) overriden values back to original
        actual.set(RunSettingsId::RUN_START_TIME, settings.get<Float>(RunSettingsId::RUN_START_TIME));
        actual.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP,
            settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP));
        actual.set(
            RunSettingsId::RUN_OUTPUT_FIRST_INDEX, settings.get<int>(RunSettingsId::RUN_OUTPUT_FIRST_INDEX));
    }
    return actual;
}

static void addTimeSteppingCategory(VirtualSettings& connector, RunSettings& settings, bool& resumeRun) {
    auto courantEnabler = [&settings] {
        Flags<TimeStepCriterionEnum> criteria =
            settings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
        return criteria.has(TimeStepCriterionEnum::COURANT);
    };

    VirtualSettings::Category& rangeCat = connector.addCategory("Integration");
    rangeCat.connect<Float>("Duration [s]", settings, RunSettingsId::RUN_END_TIME);
    rangeCat.connect("Use start time of input", "is_resumed", resumeRun);
    rangeCat.connect<Float>("Maximal timestep [s]", settings, RunSettingsId::TIMESTEPPING_MAX_TIMESTEP);
    rangeCat.connect<Float>("Initial timestep [s]", settings, RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    rangeCat.connect<EnumWrapper>("Integrator", settings, RunSettingsId::TIMESTEPPING_INTEGRATOR);
    rangeCat.connect<Flags<TimeStepCriterionEnum>>(
        "Time step criteria", settings, RunSettingsId::TIMESTEPPING_CRITERION);
    rangeCat.connect<Float>("Courant number", settings, RunSettingsId::TIMESTEPPING_COURANT_NUMBER)
        .setEnabler(courantEnabler);
    rangeCat.connect<Float>("Time step multiplier", settings, RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
}

static void addGravityCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& gravityCat = connector.addCategory("Gravity");
    gravityCat.connect<EnumWrapper>("Gravity solver", settings, RunSettingsId::GRAVITY_SOLVER);
    gravityCat.connect<Float>("Opening angle", settings, RunSettingsId::GRAVITY_OPENING_ANGLE)
        .setEnabler([&settings] {
            return settings.get<GravityEnum>(RunSettingsId::GRAVITY_SOLVER) == GravityEnum::BARNES_HUT;
        });
    gravityCat.connect<int>("Multipole order", settings, RunSettingsId::GRAVITY_MULTIPOLE_ORDER);
    gravityCat.connect<EnumWrapper>("Softening kernel", settings, RunSettingsId::GRAVITY_KERNEL);
    gravityCat.connect<Float>(
        "Recomputation period [s]", settings, RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD);
}

static void addOutputCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<EnumWrapper>("Format", settings, RunSettingsId::RUN_OUTPUT_TYPE)
        .setAccessor([&settings](const IVirtualEntry::Value& value) {
            const IoEnum type = IoEnum(value.get<EnumWrapper>());
            Path name = Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
            if (Optional<std::string> extension = getIoExtension(type)) {
                name.replaceExtension(extension.value());
            }
            settings.set(RunSettingsId::RUN_OUTPUT_NAME, name.native());
        });
    outputCat.connect<Path>("Directory", settings, RunSettingsId::RUN_OUTPUT_PATH);
    outputCat.connect<std::string>("File mask", settings, RunSettingsId::RUN_OUTPUT_NAME);
    outputCat.connect<Flags<OutputQuantityFlag>>("Quantities", settings, RunSettingsId::RUN_OUTPUT_QUANTITIES)
        .setEnabler([&settings] {
            const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
            return type == IoEnum::TEXT_FILE || type == IoEnum::VTK_FILE;
        });
    outputCat.connect<Float>("Output interval [s]", settings, RunSettingsId::RUN_OUTPUT_INTERVAL);
}

static void addLoggerCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& loggerCat = connector.addCategory("Logging");
    loggerCat.connect<EnumWrapper>("Logger", settings, RunSettingsId::RUN_LOGGER);
    loggerCat.connect<Path>("Log file", settings, RunSettingsId::RUN_LOGGER_FILE).setEnabler([&settings] {
        return settings.get<LoggerEnum>(RunSettingsId::RUN_LOGGER) == LoggerEnum::FILE;
    });
}


class SphRun : public IRun {
protected:
    SharedPtr<IDomain> domain;

public:
    explicit SphRun(const RunSettings& run, SharedPtr<IDomain> domain)
        : domain(domain) {
        settings = run;
        scheduler = Factory::getScheduler(settings);
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        AutoPtr<IBoundaryCondition> bc = Factory::getBoundaryConditions(settings, domain);
        solver = Factory::getSolver(*scheduler, settings, std::move(bc));

        for (Size matId = 0; matId < storage->getMaterialCnt(); ++matId) {
            solver->create(*storage, storage->getMaterial(matId));
        }
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        // last dump after simulation ends
        output->dump(storage, stats);
    }
};

SphJob::SphJob(const std::string& name, const RunSettings& overrides)
    : IRunJob(name) {
    settings = getDefaultSettings(name);

    settings.addEntries(overrides);
}

RunSettings SphJob::getDefaultSettings(const std::string& name) {
    const Size dumpCnt = 10;
    const Interval timeRange(0, 10);

    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
        .set(RunSettingsId::RUN_START_TIME, timeRange.lower())
        .set(RunSettingsId::RUN_END_TIME, timeRange.upper())
        .set(RunSettingsId::RUN_NAME, name)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, timeRange.size() / dumpCnt)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::NONE)
        .set(RunSettingsId::RUN_OUTPUT_NAME, getIdentifier(name) + "_%d.ssf")
        .set(RunSettingsId::RUN_VERBOSE_NAME, getIdentifier(name) + ".log")
        .set(RunSettingsId::SPH_SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_SOLVER_FORCES,
            ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS | ForceEnum::SELF_GRAVITY)
        .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 5._f)
        .set(RunSettingsId::FINDER_LEAF_SIZE, 20)
        .set(RunSettingsId::SPH_STABILIZATION_DAMPING, 0.1_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 1000)
        .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, EMPTY_FLAGS)
        .set(RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP, false)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::RUN_DIAGNOSTICS_INTERVAL, 1._f);
    return settings;
}

VirtualSettings SphJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addTimeSteppingCategory(connector, settings, isResumed);

    auto stressEnabler = [this] {
        return settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES).has(ForceEnum::SOLID_STRESS);
    };
    auto avEnabler = [this] {
        return settings.get<ArtificialViscosityEnum>(RunSettingsId::SPH_AV_TYPE) !=
               ArtificialViscosityEnum::NONE;
    };
    auto asEnabler = [this] { return settings.get<bool>(RunSettingsId::SPH_AV_USE_STRESS); };
    // auto acEnabler = [this] { return settings.get<bool>(RunSettingsId::SPH_USE_AC); };
    auto deltaSphEnabler = [this] { return settings.get<bool>(RunSettingsId::SPH_USE_DELTASPH); };
    auto enforceEnabler = [this] {
        return settings.getFlags<SmoothingLengthEnum>(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH)
            .has(SmoothingLengthEnum::SOUND_SPEED_ENFORCING);
    };

    VirtualSettings::Category& solverCat = connector.addCategory("SPH solver");
    solverCat.connect<Flags<ForceEnum>>("Forces", settings, RunSettingsId::SPH_SOLVER_FORCES);
    solverCat.connect<Vector>("Constant acceleration", settings, RunSettingsId::FRAME_CONSTANT_ACCELERATION);
    solverCat.connect<Float>("Tides mass [M_earth]", settings, RunSettingsId::FRAME_TIDES_MASS)
        .setUnits(Constants::M_earth);
    solverCat.connect<Vector>("Tides position [R_earth]", settings, RunSettingsId::FRAME_TIDES_POSITION)
        .setUnits(Constants::R_earth);
    solverCat.connect<EnumWrapper>("Solver type", settings, RunSettingsId::SPH_SOLVER_TYPE);
    solverCat.connect<EnumWrapper>("SPH discretization", settings, RunSettingsId::SPH_DISCRETIZATION);
    solverCat.connect<Flags<SmoothingLengthEnum>>(
        "Adaptive smoothing length", settings, RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH);
    solverCat.connect<Float>("Minimal smoothing length", settings, RunSettingsId::SPH_SMOOTHING_LENGTH_MIN)
        .setEnabler([this] {
            return settings.getFlags<SmoothingLengthEnum>(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH) !=
                   EMPTY_FLAGS;
        });
    solverCat
        .connect<Float>("Neighbor count enforcing strength", settings, RunSettingsId::SPH_NEIGHBOUR_ENFORCING)
        .setEnabler(enforceEnabler);
    solverCat.connect<Interval>("Neighbor range", settings, RunSettingsId::SPH_NEIGHBOUR_RANGE)
        .setEnabler(enforceEnabler);
    solverCat
        .connect<bool>("Use radii hash map", settings, RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP)
        .setEnabler([this] {
            return settings.get<SolverEnum>(RunSettingsId::SPH_SOLVER_TYPE) == SolverEnum::ASYMMETRIC_SOLVER;
        });
    solverCat
        .connect<bool>("Apply correction tensor", settings, RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR)
        .setEnabler(stressEnabler);
    solverCat.connect<bool>("Sum only undamaged particles", settings, RunSettingsId::SPH_SUM_ONLY_UNDAMAGED);
    solverCat.connect<EnumWrapper>("Neighbour finder", settings, RunSettingsId::SPH_FINDER);
    solverCat.connect<EnumWrapper>("Boundary condition", settings, RunSettingsId::DOMAIN_BOUNDARY);

    VirtualSettings::Category& avCat = connector.addCategory("Artificial viscosity");
    avCat.connect<EnumWrapper>("Artificial viscosity type", settings, RunSettingsId::SPH_AV_TYPE);
    avCat.connect<bool>("Apply Balsara switch", settings, RunSettingsId::SPH_AV_USE_BALSARA)
        .setEnabler(avEnabler);
    avCat.connect<Float>("Artificial viscosity alpha", settings, RunSettingsId::SPH_AV_ALPHA)
        .setEnabler(avEnabler);
    avCat.connect<Float>("Artificial viscosity beta", settings, RunSettingsId::SPH_AV_BETA)
        .setEnabler(avEnabler);
    avCat.connect<bool>("Apply artificial stress", settings, RunSettingsId::SPH_AV_USE_STRESS);
    avCat.connect<Float>("Artificial stress factor", settings, RunSettingsId::SPH_AV_STRESS_FACTOR)
        .setEnabler(asEnabler);
    avCat.connect<Float>("Artificial stress exponent", settings, RunSettingsId::SPH_AV_STRESS_EXPONENT)
        .setEnabler(asEnabler);
    avCat.connect<bool>("Apply artificial conductivity", settings, RunSettingsId::SPH_USE_AC);

    VirtualSettings::Category& modCat = connector.addCategory("SPH modifications");
    modCat.connect<bool>("Enable XPSH", settings, RunSettingsId::SPH_USE_XSPH);
    modCat.connect<Float>("XSPH epsilon", settings, RunSettingsId::SPH_XSPH_EPSILON).setEnabler([this] {
        return settings.get<bool>(RunSettingsId::SPH_USE_XSPH);
    });
    modCat.connect<bool>("Enable delta-SPH", settings, RunSettingsId::SPH_USE_DELTASPH);
    modCat.connect<Float>("delta-SPH alpha", settings, RunSettingsId::SPH_VELOCITY_DIFFUSION_ALPHA)
        .setEnabler(deltaSphEnabler);
    modCat.connect<Float>("delta-SPH delta", settings, RunSettingsId::SPH_DENSITY_DIFFUSION_DELTA)
        .setEnabler(deltaSphEnabler);

    VirtualSettings::Category& scriptCat = connector.addCategory("Scripts");
    scriptCat.connect<bool>("Enable script", settings, RunSettingsId::SPH_SCRIPT_ENABLE);
    scriptCat.connect<Path>("Script file", settings, RunSettingsId::SPH_SCRIPT_FILE).setEnabler([this] {
        return settings.get<bool>(RunSettingsId::SPH_SCRIPT_ENABLE);
    });

    addGravityCategory(connector, settings);
    addOutputCategory(connector, settings);
    addLoggerCategory(connector, settings);

    return connector;
}

AutoPtr<IRun> SphJob::getRun(const RunSettings& overrides) const {
    ASSERT(overrides.size() < 15); // not really required, just checking that we don't override everything
    const BoundaryEnum boundary = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    SharedPtr<IDomain> domain;
    if (boundary != BoundaryEnum::NONE) {
        domain = this->getInput<IDomain>("boundary");
    }

    RunSettings run = overrideSettings(settings, overrides, isResumed);
    if (!run.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES).has(ForceEnum::SOLID_STRESS)) {
        run.set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, false);
    }

    return makeAuto<SphRun>(run, domain);
}

static JobRegistrar sRegisterSph(
    "SPH run",
    "simulations",
    [](const std::string& name) { return makeAuto<SphJob>(name, EMPTY_SETTINGS); },
    "Runs a SPH simulation, using provided initial conditions.");

// ----------------------------------------------------------------------------------------------------------
// SphStabilizationWorker
// ----------------------------------------------------------------------------------------------------------

class SphStabilizationRun : public SphRun {
public:
    using SphRun::SphRun;

    virtual void setUp(SharedPtr<Storage> storage) override {
        AutoPtr<IBoundaryCondition> bc = Factory::getBoundaryConditions(settings, domain);
        solver = makeAuto<StabilizationSolver>(*scheduler, settings, std::move(bc));

        for (Size matId = 0; matId < storage->getMaterialCnt(); ++matId) {
            solver->create(*storage, storage->getMaterial(matId));
        }
    }
};

VirtualSettings SphStabilizationJob::getSettings() {
    VirtualSettings connector = SphJob::getSettings();

    VirtualSettings::Category& stabCat = connector.addCategory("Stabilization");
    stabCat.connect<Float>("Damping coefficient", settings, RunSettingsId::SPH_STABILIZATION_DAMPING);

    return connector;
}

AutoPtr<IRun> SphStabilizationJob::getRun(const RunSettings& overrides) const {
    RunSettings run = overrideSettings(settings, overrides, isResumed);
    const BoundaryEnum boundary = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    SharedPtr<IDomain> domain;
    if (boundary != BoundaryEnum::NONE) {
        domain = this->getInput<IDomain>("boundary");
    }
    return makeAuto<SphStabilizationRun>(run, domain);
}

static JobRegistrar sRegisterSphStab(
    "SPH stabilization",
    "stabilization",
    "simulations",
    [](const std::string& name) { return makeAuto<SphStabilizationJob>(name, EMPTY_SETTINGS); },
    "Runs a SPH simulation with a damping term, suitable for stabilization of non-equilibrium initial "
    "conditions.");


// ----------------------------------------------------------------------------------------------------------
// NBodyWorker
// ----------------------------------------------------------------------------------------------------------

class NBodyRun : public IRun {
public:
    NBodyRun(const RunSettings& run) {
        settings = run;
        scheduler = Factory::getScheduler(settings);
    }

    virtual void setUp(SharedPtr<Storage> storage) override {
        logger = Factory::getLogger(settings);

        const bool aggregateEnable = settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE);
        const AggregateEnum aggregateSource =
            settings.get<AggregateEnum>(RunSettingsId::NBODY_AGGREGATES_SOURCE);
        if (aggregateEnable) {
            AutoPtr<AggregateSolver> aggregates = makeAuto<AggregateSolver>(*scheduler, settings);
            aggregates->createAggregateData(*storage, aggregateSource);
            solver = std::move(aggregates);
        } else {
            solver = makeAuto<NBodySolver>(*scheduler, settings);
        }

        NullMaterial mtl(BodySettings::getDefaults());
        solver->create(*storage, mtl);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        output->dump(storage, stats);
    }
};

NBodyJob::NBodyJob(const std::string& name, const RunSettings& overrides)
    : IRunJob(name) {

    settings = getDefaultSettings(name);
    settings.addEntries(overrides);
}

RunSettings NBodyJob::getDefaultSettings(const std::string& name) {
    const Interval timeRange(0, 1.e6_f);
    RunSettings settings;
    settings.set(RunSettingsId::RUN_NAME, name)
        .set(RunSettingsId::RUN_TYPE, RunTypeEnum::NBODY)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::RUN_START_TIME, timeRange.lower())
        .set(RunSettingsId::RUN_END_TIME, timeRange.upper())
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, timeRange.size() / 10)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::NONE)
        .set(RunSettingsId::RUN_OUTPUT_NAME, getIdentifier(name) + "_%d.ssf")
        .set(RunSettingsId::RUN_VERBOSE_NAME, getIdentifier(name) + ".log")
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::FINDER_LEAF_SIZE, 20)
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
    return settings;
}

VirtualSettings NBodyJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addTimeSteppingCategory(connector, settings, isResumed);
    addGravityCategory(connector, settings);

    VirtualSettings::Category& aggregateCat = connector.addCategory("Aggregates (experimental)");
    aggregateCat.connect<bool>("Enable", settings, RunSettingsId::NBODY_AGGREGATES_ENABLE);
    aggregateCat.connect<EnumWrapper>("Initial aggregates", settings, RunSettingsId::NBODY_AGGREGATES_SOURCE)
        .setEnabler([this] { return settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE); });

    auto collisionEnabler = [this] { return !settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE); };
    auto mergeEnabler = [this] {
        const bool aggregates = settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE);
        const CollisionHandlerEnum handler =
            settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER);
        return aggregates || handler != CollisionHandlerEnum::ELASTIC_BOUNCE;
    };

    VirtualSettings::Category& collisionCat = connector.addCategory("Collisions");
    collisionCat.connect<EnumWrapper>("Collision handler", settings, RunSettingsId::COLLISION_HANDLER)
        .setEnabler(collisionEnabler);
    collisionCat.connect<EnumWrapper>("Overlap handler", settings, RunSettingsId::COLLISION_OVERLAP)
        .setEnabler(collisionEnabler);
    collisionCat.connect<Float>("Normal restitution", settings, RunSettingsId::COLLISION_RESTITUTION_NORMAL)
        .setEnabler(collisionEnabler);
    collisionCat
        .connect<Float>("Tangential restitution", settings, RunSettingsId::COLLISION_RESTITUTION_TANGENT)
        .setEnabler(collisionEnabler);
    collisionCat.connect<Float>("Merge velocity limit", settings, RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT)
        .setEnabler(mergeEnabler);
    collisionCat
        .connect<Float>("Merge rotation limit", settings, RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT)
        .setEnabler(mergeEnabler);

    addLoggerCategory(connector, settings);
    addOutputCategory(connector, settings);
    return connector;
}

AutoPtr<IRun> NBodyJob::getRun(const RunSettings& overrides) const {
    RunSettings run = overrideSettings(settings, overrides, isResumed);
    return makeAuto<NBodyRun>(run);
}

static JobRegistrar sRegisterNBody(
    "N-body run",
    "simulations",
    [](const std::string& name) { return makeAuto<NBodyJob>(name, EMPTY_SETTINGS); },
    "Runs N-body simulation using given initial conditions.");


NAMESPACE_SPH_END
