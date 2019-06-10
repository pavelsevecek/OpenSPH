#include "run/workers/SimulationWorkers.h"
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
    rangeCat.connect<Float>("Maximal timestep [s]", settings, RunSettingsId::TIMESTEPPING_MAX_TIMESTEP)
        .connect<Float>("Initial timestep [s]", settings, RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP)
        .connect<EnumWrapper>("Integrator", settings, RunSettingsId::TIMESTEPPING_INTEGRATOR)
        .connect<Flags<TimeStepCriterionEnum>>(
            "Time step criteria", settings, RunSettingsId::TIMESTEPPING_CRITERION)
        .connect<Float>(
            "Courant number", settings, RunSettingsId::TIMESTEPPING_COURANT_NUMBER, courantEnabler)
        .connect<Float>("Time step multiplier", settings, RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
}

static void addGravityCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& gravityCat = connector.addCategory("Gravity");
    gravityCat.connect<EnumWrapper>("Gravity solver", settings, RunSettingsId::GRAVITY_SOLVER)
        .connect<Float>("Opening angle",
            settings,
            RunSettingsId::GRAVITY_OPENING_ANGLE,
            [&settings] {
                return settings.get<GravityEnum>(RunSettingsId::GRAVITY_SOLVER) == GravityEnum::BARNES_HUT;
            })
        .connect<int>("Multipole order", settings, RunSettingsId::GRAVITY_MULTIPOLE_ORDER)
        .connect<EnumWrapper>("Softening kernel", settings, RunSettingsId::GRAVITY_KERNEL)
        .connect<Float>("Recomputation period [s]", settings, RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD);
}

static void addOutputCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<EnumWrapper>("Format", settings, RunSettingsId::RUN_OUTPUT_TYPE)
        .connect<Path>("Directory", settings, RunSettingsId::RUN_OUTPUT_PATH)
        .connect<std::string>("File mask", settings, RunSettingsId::RUN_OUTPUT_NAME)
        .connect<Flags<OutputQuantityFlag>>("Quantities",
            settings,
            RunSettingsId::RUN_OUTPUT_QUANTITIES,
            [&settings] {
                const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
                return type == IoEnum::TEXT_FILE || type == IoEnum::VTK_FILE;
            })
        .connect<Float>("Output interval [s]", settings, RunSettingsId::RUN_OUTPUT_INTERVAL);
}

static void addLoggerCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& loggerCat = connector.addCategory("Logging");
    loggerCat.connect<EnumWrapper>("Logger", settings, RunSettingsId::RUN_LOGGER)
        .connect<Path>("File", settings, RunSettingsId::RUN_LOGGER_FILE, [&settings] {
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

SphWorker::SphWorker(const std::string& name, const RunSettings& overrides)
    : IRunWorker(name) {
    settings = getDefaultSettings(name);

    settings.addEntries(overrides);
}

RunSettings SphWorker::getDefaultSettings(const std::string& name) {
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
        .set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::RUN_DIAGNOSTICS_INTERVAL, 1._f);
    return settings;
}

VirtualSettings SphWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addTimeSteppingCategory(connector, settings, isResumed);

    auto treeEnabler = [this] {
        return settings.get<FinderEnum>(RunSettingsId::SPH_FINDER) == FinderEnum::KD_TREE ||
               settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES).has(ForceEnum::SELF_GRAVITY);
    };

    auto stressEnabler = [this] {
        return settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES).has(ForceEnum::SOLID_STRESS);
    };

    VirtualSettings::Category& solverCat = connector.addCategory("SPH solver");
    solverCat.connect<Flags<ForceEnum>>("Forces", settings, RunSettingsId::SPH_SOLVER_FORCES)
        .connect<Vector>("Constant acceleration", settings, RunSettingsId::FRAME_CONSTANT_ACCELERATION)
        .connect<EnumWrapper>("Artificial viscosity", settings, RunSettingsId::SPH_AV_TYPE)
        .connect<bool>("Apply Balsara switch", settings, RunSettingsId::SPH_AV_USE_BALSARA)
        .connect<bool>("Apply artificial stress", settings, RunSettingsId::SPH_AV_USE_STRESS)
        .connect<Float>("Artificial viscosity alpha", settings, RunSettingsId::SPH_AV_ALPHA)
        .connect<Float>("Artificial viscosity beta", settings, RunSettingsId::SPH_AV_BETA)
        .connect<EnumWrapper>("Solver type", settings, RunSettingsId::SPH_SOLVER_TYPE)
        .connect<EnumWrapper>("SPH discretization", settings, RunSettingsId::SPH_DISCRETIZATION)
        .connect<bool>("Apply correction tensor",
            settings,
            RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR,
            stressEnabler)
        .connect<bool>("Sum only undamaged particles", settings, RunSettingsId::SPH_SUM_ONLY_UNDAMAGED)
        .connect<EnumWrapper>("Neighbour finder", settings, RunSettingsId::SPH_FINDER)
        .connect<int>("Max leaf size", settings, RunSettingsId::FINDER_LEAF_SIZE, treeEnabler)
        .connect<int>("Max parallel depth", settings, RunSettingsId::FINDER_MAX_PARALLEL_DEPTH, treeEnabler)
        .connect<EnumWrapper>("Boundary condition", settings, RunSettingsId::DOMAIN_BOUNDARY);

    addGravityCategory(connector, settings);
    addLoggerCategory(connector, settings);
    addOutputCategory(connector, settings);

    return connector;
}

AutoPtr<IRun> SphWorker::getRun(const RunSettings& overrides) const {
    ASSERT(overrides.size() < 10); // not really required, just checking that we don't override everything
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

static WorkerRegistrar sRegisterSph("SPH run", "simulations", [](const std::string& name) {
    return makeAuto<SphWorker>(name, EMPTY_SETTINGS);
});

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

VirtualSettings SphStabilizationWorker::getSettings() {
    VirtualSettings connector = SphWorker::getSettings();

    VirtualSettings::Category& stabCat = connector.addCategory("Stabilization");
    stabCat.connect<Float>("Damping coefficient", settings, RunSettingsId::SPH_STABILIZATION_DAMPING);

    return connector;
}

AutoPtr<IRun> SphStabilizationWorker::getRun(const RunSettings& overrides) const {
    RunSettings run = overrideSettings(settings, overrides, isResumed);
    const BoundaryEnum boundary = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    SharedPtr<IDomain> domain;
    if (boundary != BoundaryEnum::NONE) {
        domain = this->getInput<IDomain>("boundary");
    }
    return makeAuto<SphStabilizationRun>(run, domain);
}

static WorkerRegistrar sRegisterSphStab("SPH stabilization",
    "stabilization",
    "simulations",
    [](const std::string& name) { return makeAuto<SphStabilizationWorker>(name, EMPTY_SETTINGS); });


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

NBodyWorker::NBodyWorker(const std::string& name, const RunSettings& overrides)
    : IRunWorker(name) {

    settings = getDefaultSettings(name);
    settings.addEntries(overrides);
}

RunSettings NBodyWorker::getDefaultSettings(const std::string& name) {
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

VirtualSettings NBodyWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addTimeSteppingCategory(connector, settings, isResumed);
    addGravityCategory(connector, settings);

    VirtualSettings::Category& aggregateCat = connector.addCategory("Aggregates (experimental)");
    aggregateCat.connect<bool>("Enable", settings, RunSettingsId::NBODY_AGGREGATES_ENABLE)
        .connect<EnumWrapper>("Initial aggregates", settings, RunSettingsId::NBODY_AGGREGATES_SOURCE, [this] {
            return settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE);
        });

    auto collisionEnabler = [this] { return !settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE); };
    auto mergeEnabler = [this] {
        const bool aggregates = settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE);
        const CollisionHandlerEnum handler =
            settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER);
        return aggregates || handler != CollisionHandlerEnum::ELASTIC_BOUNCE;
    };

    VirtualSettings::Category& collisionCat = connector.addCategory("Collisions");
    collisionCat
        .connect<EnumWrapper>(
            "Collision handler", settings, RunSettingsId::COLLISION_HANDLER, collisionEnabler)
        .connect<EnumWrapper>("Overlap handler", settings, RunSettingsId::COLLISION_OVERLAP, collisionEnabler)
        .connect<Float>(
            "Normal restitution", settings, RunSettingsId::COLLISION_RESTITUTION_NORMAL, collisionEnabler)
        .connect<Float>("Tangential restitution",
            settings,
            RunSettingsId::COLLISION_RESTITUTION_TANGENT,
            collisionEnabler)
        .connect<Float>(
            "Merge velocity limit", settings, RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT, mergeEnabler)
        .connect<Float>(
            "Merge rotation limit", settings, RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT, mergeEnabler);

    addLoggerCategory(connector, settings);
    addOutputCategory(connector, settings);
    return connector;
}

AutoPtr<IRun> NBodyWorker::getRun(const RunSettings& overrides) const {
    RunSettings run = overrideSettings(settings, overrides, isResumed);
    return makeAuto<NBodyRun>(run);
}

static WorkerRegistrar sRegisterNBody("N-body run", "simulations", [](const std::string& name) {
    return makeAuto<NBodyWorker>(name, EMPTY_SETTINGS);
});


NAMESPACE_SPH_END
