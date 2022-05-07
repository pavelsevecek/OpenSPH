#include "run/jobs/SimulationJobs.h"
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

static String getIdentifier(const String& name) {
    String escaped = name;
    escaped.replaceAll(" ", "-");
    return escaped.toLowercase();
}

// ----------------------------------------------------------------------------------------------------------
// SphJob
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
    auto derivativeEnabler = [&settings] {
        Flags<TimeStepCriterionEnum> criteria =
            settings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
        return criteria.hasAny(TimeStepCriterionEnum::DERIVATIVES, TimeStepCriterionEnum::ACCELERATION);
    };
    auto divergenceEnabler = [&settings] {
        Flags<TimeStepCriterionEnum> criteria =
            settings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
        return criteria.has(TimeStepCriterionEnum::DIVERGENCE);
    };

    VirtualSettings::Category& rangeCat = connector.addCategory("Integration");
    rangeCat.connect<Float>("Duration [s]", settings, RunSettingsId::RUN_END_TIME);
    rangeCat.connect("Use start time of input", "is_resumed", resumeRun)
        .setTooltip(
            "If the simulation continues from a saved state, start from the time of the input instead of "
            "zero.");
    rangeCat.connect<Float>("Maximal timestep [s]", settings, RunSettingsId::TIMESTEPPING_MAX_TIMESTEP);
    rangeCat.connect<Float>("Initial timestep [s]", settings, RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    rangeCat.connect<EnumWrapper>("Integrator", settings, RunSettingsId::TIMESTEPPING_INTEGRATOR);
    rangeCat.connect<Flags<TimeStepCriterionEnum>>(
        "Time step criteria", settings, RunSettingsId::TIMESTEPPING_CRITERION);
    rangeCat.connect<Float>("Courant number", settings, RunSettingsId::TIMESTEPPING_COURANT_NUMBER)
        .setEnabler(courantEnabler);
    rangeCat.connect<Float>("Derivative factor", settings, RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR)
        .setEnabler(derivativeEnabler);
    rangeCat.connect<Float>("Divergence factor", settings, RunSettingsId::TIMESTEPPING_DIVERGENCE_FACTOR)
        .setEnabler(divergenceEnabler);
    rangeCat.connect<Float>("Max step change", settings, RunSettingsId::TIMESTEPPING_MAX_INCREASE);
    rangeCat.connect<bool>("Save particle time steps", settings, RunSettingsId::SAVE_PARTICLE_TIMESTEPS);
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

static void addOutputCategory(VirtualSettings& connector, RunSettings& settings, const SharedToken& owner) {
    auto enabler = [&settings] {
        const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
        return type != IoEnum::NONE;
    };

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<EnumWrapper>("Format", settings, RunSettingsId::RUN_OUTPUT_TYPE)
        .setValidator([](const IVirtualEntry::Value& value) {
            const IoEnum type = IoEnum(value.get<EnumWrapper>());
            return type == IoEnum::NONE || getIoCapabilities(type).has(IoCapability::OUTPUT);
        })
        .addAccessor(owner,
            [&settings](const IVirtualEntry::Value& value) {
                const IoEnum type = IoEnum(value.get<EnumWrapper>());
                Path name = Path(settings.get<String>(RunSettingsId::RUN_OUTPUT_NAME));
                if (Optional<String> extension = getIoExtension(type)) {
                    name.replaceExtension(extension.value());
                }
                settings.set(RunSettingsId::RUN_OUTPUT_NAME, name.string());
            })
        .setSideEffect(); // needs to update the 'File mask' entry
    outputCat.connect<Path>("Directory", settings, RunSettingsId::RUN_OUTPUT_PATH)
        .setEnabler(enabler)
        .setPathType(IVirtualEntry::PathType::DIRECTORY);
    outputCat.connect<String>("File mask", settings, RunSettingsId::RUN_OUTPUT_NAME).setEnabler(enabler);
    outputCat.connect<Flags<OutputQuantityFlag>>("Quantities", settings, RunSettingsId::RUN_OUTPUT_QUANTITIES)
        .setEnabler([&settings] {
            const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
            return type == IoEnum::TEXT_FILE || type == IoEnum::VTK_FILE;
        });
    outputCat.connect<EnumWrapper>("Output spacing", settings, RunSettingsId::RUN_OUTPUT_SPACING)
        .setEnabler(enabler);
    outputCat.connect<Float>("Output interval [s]", settings, RunSettingsId::RUN_OUTPUT_INTERVAL)
        .setEnabler([&] {
            const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
            const OutputSpacing spacing = settings.get<OutputSpacing>(RunSettingsId::RUN_OUTPUT_SPACING);
            return type != IoEnum::NONE && spacing != OutputSpacing::CUSTOM;
        });
    outputCat.connect<String>("Custom times [s]", settings, RunSettingsId::RUN_OUTPUT_CUSTOM_TIMES)
        .setEnabler([&] {
            const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
            const OutputSpacing spacing = settings.get<OutputSpacing>(RunSettingsId::RUN_OUTPUT_SPACING);
            return type != IoEnum::NONE && spacing == OutputSpacing::CUSTOM;
        });
}

static void addLoggerCategory(VirtualSettings& connector, RunSettings& settings) {
    VirtualSettings::Category& loggerCat = connector.addCategory("Logging");
    loggerCat.connect<EnumWrapper>("Logger", settings, RunSettingsId::RUN_LOGGER);
    loggerCat.connect<Path>("Log file", settings, RunSettingsId::RUN_LOGGER_FILE)
        .setPathType(IVirtualEntry::PathType::OUTPUT_FILE)
        .setEnabler(
            [&settings] { return settings.get<LoggerEnum>(RunSettingsId::RUN_LOGGER) == LoggerEnum::FILE; });
    loggerCat.connect<int>("Log verbosity", settings, RunSettingsId::RUN_LOGGER_VERBOSITY);
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

SphJob::SphJob(const String& name, const RunSettings& overrides)
    : IRunJob(name) {
    settings = getDefaultSettings(name);

    settings.addEntries(overrides);
}

RunSettings SphJob::getDefaultSettings(const String& name) {
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
    solverCat.connect<EnumWrapper>("Solver type", settings, RunSettingsId::SPH_SOLVER_TYPE);
    solverCat.connect<EnumWrapper>("SPH discretization", settings, RunSettingsId::SPH_DISCRETIZATION);
    solverCat.connect<Flags<SmoothingLengthEnum>>(
        "Adaptive smoothing length", settings, RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH);
    solverCat
        .connect<Interval>(
            "Allowed smoothing length range [m]", settings, RunSettingsId::SPH_SMOOTHING_LENGTH_RANGE)
        .setEnabler([this] {
            return settings.getFlags<SmoothingLengthEnum>(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH) !=
                   EMPTY_FLAGS;
        });
    solverCat
        .connect<Float>("Neighbor count enforcing strength", settings, RunSettingsId::SPH_NEIGHBOR_ENFORCING)
        .setEnabler(enforceEnabler);
    solverCat.connect<Interval>("Neighbor range", settings, RunSettingsId::SPH_NEIGHBOR_RANGE)
        .setEnabler(enforceEnabler);
    solverCat
        .connect<bool>("Use radii hash map", settings, RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP)
        .setEnabler([this] {
            return settings.get<SolverEnum>(RunSettingsId::SPH_SOLVER_TYPE) == SolverEnum::ASYMMETRIC_SOLVER;
        });
    solverCat.connect<int>("Iteration count", settings, RunSettingsId::SPH_POSITION_BASED_ITERATION_COUNT)
        .setEnabler([this] {
            return settings.get<SolverEnum>(RunSettingsId::SPH_SOLVER_TYPE) == SolverEnum::POSITION_BASED;
        });
    solverCat
        .connect<bool>("Apply correction tensor", settings, RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR)
        .setEnabler(stressEnabler);
    solverCat.connect<bool>("Sum only undamaged particles", settings, RunSettingsId::SPH_SUM_ONLY_UNDAMAGED);
    solverCat.connect<EnumWrapper>("Continuity mode", settings, RunSettingsId::SPH_CONTINUITY_MODE);
    solverCat.connect<EnumWrapper>("Neighbor finder", settings, RunSettingsId::SPH_FINDER);
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
    avCat.connect<EnumWrapper>("Signal speed", settings, RunSettingsId::SPH_AC_SIGNAL_SPEED)
        .setEnabler([this] { return settings.get<bool>(RunSettingsId::SPH_USE_AC); });

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

    auto scriptEnabler = [this] { return settings.get<bool>(RunSettingsId::SPH_SCRIPT_ENABLE); };

    VirtualSettings::Category& scriptCat = connector.addCategory("Scripts");
    scriptCat.connect<bool>("Enable script", settings, RunSettingsId::SPH_SCRIPT_ENABLE);
    scriptCat.connect<Path>("Script file", settings, RunSettingsId::SPH_SCRIPT_FILE)
        .setEnabler(scriptEnabler)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats({ { "Chaiscript script", "chai" } });
    scriptCat.connect<Float>("Script period [s]", settings, RunSettingsId::SPH_SCRIPT_PERIOD)
        .setEnabler(scriptEnabler);
    scriptCat.connect<bool>("Run only once", settings, RunSettingsId::SPH_SCRIPT_ONESHOT)
        .setEnabler(scriptEnabler);

    addGravityCategory(connector, settings);
    addOutputCategory(connector, settings, *this);
    addLoggerCategory(connector, settings);

    return connector;
}

AutoPtr<IRun> SphJob::getRun(const RunSettings& overrides) const {
    SPH_ASSERT(overrides.size() < 20); // not really required, just checking that we don't override everything
    const BoundaryEnum boundary = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    SharedPtr<IDomain> domain;
    if (boundary != BoundaryEnum::NONE) {
        domain = this->getInput<IDomain>("boundary");
    }

    RunSettings run = overrideSettings(settings, overrides, isResumed);
    if (!run.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES).has(ForceEnum::SOLID_STRESS)) {
        run.set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, false);
    }
    const IoEnum output = run.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
    const OutputSpacing spacing = run.get<OutputSpacing>(RunSettingsId::RUN_OUTPUT_SPACING);
    if (output != IoEnum::NONE && spacing == OutputSpacing::LINEAR) {
        const Float maxTimeStep = run.get<Float>(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP);
        const Float outputInterval = run.get<Float>(RunSettingsId::RUN_OUTPUT_INTERVAL);
        if (maxTimeStep > outputInterval) {
            throw InvalidSetup(
                "Output interval is larger than the maximal time step. This could cause inconsistent "
                "simulation speed in the output file sequence.");
        }
    }

    return makeAuto<SphRun>(run, domain);
}

static JobRegistrar sRegisterSph(
    "SPH run",
    "simulations",
    [](const String& name) { return makeAuto<SphJob>(name, EMPTY_SETTINGS); },
    "Runs a SPH simulation, using provided initial conditions.");

// ----------------------------------------------------------------------------------------------------------
// SphStabilizationJob
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
    [](const String& name) { return makeAuto<SphStabilizationJob>(name, EMPTY_SETTINGS); },
    "Runs a SPH simulation with a damping term, suitable for stabilization of non-equilibrium initial "
    "conditions.");


// ----------------------------------------------------------------------------------------------------------
// NBodyJob
// ----------------------------------------------------------------------------------------------------------

class NBodyRun : public IRun {
private:
    bool useSoft;

public:
    NBodyRun(const RunSettings& run, const bool useSoft)
        : useSoft(useSoft) {
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
        } else if (useSoft) {
            solver = makeAuto<SoftSphereSolver>(*scheduler, settings);
        } else {
            solver = makeAuto<HardSphereSolver>(*scheduler, settings);
        }

        NullMaterial mtl(BodySettings::getDefaults());
        solver->create(*storage, mtl);

        setPersistentIndices(*storage);
    }

    virtual void tearDown(const Storage& storage, const Statistics& stats) override {
        output->dump(storage, stats);
    }
};

NBodyJob::NBodyJob(const String& name, const RunSettings& overrides)
    : IRunJob(name) {

    settings = getDefaultSettings(name);
    settings.addEntries(overrides);
}

RunSettings NBodyJob::getDefaultSettings(const String& name) {
    const Interval timeRange(0, 1.e6_f);
    RunSettings settings;
    settings.set(RunSettingsId::RUN_NAME, name)
        .set(RunSettingsId::RUN_TYPE, RunTypeEnum::NBODY)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR, 0.2_f)
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
    aggregateCat.connect<bool>("Enable aggregates", settings, RunSettingsId::NBODY_AGGREGATES_ENABLE);
    aggregateCat.connect<EnumWrapper>("Initial aggregates", settings, RunSettingsId::NBODY_AGGREGATES_SOURCE)
        .setEnabler([this] { return settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE); });

    VirtualSettings::Category& softCat = connector.addCategory("Soft-body physics (experimental)");
    softCat.connect("Enable soft-body", "soft.enable", useSoft);
    softCat.connect<Float>("Repel force strength", settings, RunSettingsId::SOFT_REPEL_STRENGTH)
        .setEnabler([this] { return useSoft; });
    softCat.connect<Float>("Friction force strength", settings, RunSettingsId::SOFT_FRICTION_STRENGTH)
        .setEnabler([this] { return useSoft; });

    auto collisionEnabler = [this] {
        return !useSoft && !settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE) &&
               settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER) !=
                   CollisionHandlerEnum::NONE;
    };
    auto restitutionEnabler = [this] {
        if (useSoft || settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE)) {
            return false;
        }
        const CollisionHandlerEnum handler =
            settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER);
        const OverlapEnum overlap = settings.get<OverlapEnum>(RunSettingsId::COLLISION_OVERLAP);
        return handler == CollisionHandlerEnum::ELASTIC_BOUNCE ||
               handler == CollisionHandlerEnum::MERGE_OR_BOUNCE || overlap == OverlapEnum::INTERNAL_BOUNCE;
    };
    auto mergeLimitEnabler = [this] {
        if (useSoft) {
            return false;
        }
        const CollisionHandlerEnum handler =
            settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER);
        if (handler == CollisionHandlerEnum::NONE) {
            return false;
        }
        const bool aggregates = settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE);
        const OverlapEnum overlap = settings.get<OverlapEnum>(RunSettingsId::COLLISION_OVERLAP);
        return aggregates || handler == CollisionHandlerEnum::MERGE_OR_BOUNCE ||
               overlap == OverlapEnum::PASS_OR_MERGE || overlap == OverlapEnum::REPEL_OR_MERGE;
    };

    VirtualSettings::Category& collisionCat = connector.addCategory("Collisions");
    collisionCat.connect<EnumWrapper>("Collision handler", settings, RunSettingsId::COLLISION_HANDLER)
        .setEnabler([this] { //
            return !useSoft && !settings.get<bool>(RunSettingsId::NBODY_AGGREGATES_ENABLE);
        });
    collisionCat.connect<EnumWrapper>("Overlap handler", settings, RunSettingsId::COLLISION_OVERLAP)
        .setEnabler(collisionEnabler);
    collisionCat.connect<Float>("Normal restitution", settings, RunSettingsId::COLLISION_RESTITUTION_NORMAL)
        .setEnabler(restitutionEnabler);
    collisionCat
        .connect<Float>("Tangential restitution", settings, RunSettingsId::COLLISION_RESTITUTION_TANGENT)
        .setEnabler(restitutionEnabler);
    collisionCat.connect<Float>("Merge velocity limit", settings, RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT)
        .setEnabler(mergeLimitEnabler);
    collisionCat
        .connect<Float>("Merge rotation limit", settings, RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT)
        .setEnabler(mergeLimitEnabler);

    addLoggerCategory(connector, settings);
    addOutputCategory(connector, settings, *this);
    return connector;
}

AutoPtr<IRun> NBodyJob::getRun(const RunSettings& overrides) const {
    RunSettings run = overrideSettings(settings, overrides, isResumed);
    if (run.get<TimesteppingEnum>(RunSettingsId::TIMESTEPPING_INTEGRATOR) ==
        TimesteppingEnum::PREDICTOR_CORRECTOR) {
        throw InvalidSetup(
            "Predictor-corrector is incompatible with N-body solver. Please select a different integrator.");
    }
    return makeAuto<NBodyRun>(run, useSoft);
}

static JobRegistrar sRegisterNBody(
    "N-body run",
    "simulations",
    [](const String& name) { return makeAuto<NBodyJob>(name, EMPTY_SETTINGS); },
    "Runs N-body simulation using given initial conditions.");


NAMESPACE_SPH_END
