/// \brief Executable running a single impact simulation, using command-line parameters

#include "Sph.h"
#include "run/Node.h"
#include "run/workers/InitialConditionWorkers.h"
#include "run/workers/IoWorkers.h"
#include "run/workers/MaterialWorkers.h"
#include "run/workers/ParticleWorkers.h"
#include "run/workers/SimulationWorkers.h"

using namespace Sph;

static Array<ArgDesc> params{
    { "tr", "target-radius", ArgEnum::FLOAT, "Radius of the target [m]" },
    { "tp", "target-period", ArgEnum::FLOAT, "Rotational period of the target [h]" },
    { "ir", "impactor-radius", ArgEnum::FLOAT, "Radius of the impactor [m]" },
    { "q",
        "impact-energy",
        ArgEnum::FLOAT,
        "Relative impact energy Q/Q_D^*. This option can be only used together with -tr and -v, it is "
        "incompatible with -ir." },
    { "v", "impact-speed", ArgEnum::FLOAT, "Impact speed [km/s]" },
    { "phi", "impact-angle", ArgEnum::FLOAT, "Impact angle [deg]" },
    { "n", "particle-count", ArgEnum::INT, "Number of particles in the target" },
    { "st", "stabilization-time", ArgEnum::FLOAT, "Duration of the stabilization phase [s]" },
    { "ft", "fragmentation-time", ArgEnum::FLOAT, "Duration of the fragmentation phase [s]" },
    { "rt", "reaccumulation-time", ArgEnum::FLOAT, "Duration of the reaccumulation phase [s]" },
    { "i", "resume-from", ArgEnum::STRING, "Resume simulation from given state file" },
    { "o",
        "output-dir",
        ArgEnum::STRING,
        "Directory containing configuration files and run output files. If not specified, it is determined "
        "from other arguments. If no arguments are specified, the current working directory is used." },
};

static void printBanner(ILogger& logger) {
    logger.write("*******************************************************************************");
    logger.write("******************************* OpenSPH Impact*********************************");
    logger.write("*******************************************************************************");
}

static void printNoConfigsMsg(ILogger& logger, const Path& outputDir) {
    logger.write("");
    logger.write("No configuration files found, the program will generate default configuration ");
    logger.write("files and save them to directory '" + outputDir.native() + "'");
    logger.write("");
    logger.write("To start a simulation, re-run this program; it will load the generated files. ");
    logger.write("You can also specify parameters of the simulation as command-line arguments.  ");
    logger.write("Note that these arguments will override parameters loaded from configuration  ");
    logger.write("files. For more information, execute the program with -h (or --help) argument.");
    logger.write("");
}

template <typename TEnum>
static Settings<TEnum> loadSettings(const Path& path,
    const Settings<TEnum>& defaults,
    ILogger& logger,
    bool& doDryRun) {
    Settings<TEnum> settings = defaults;
    const bool result = settings.tryLoadFileOrSaveCurrent(path);
    if (result) {
        logger.write("Loaded configuration file '" + path.native() + "'");

        // at least one configuration file exists, run the simulation
        doDryRun = false;
    } else {
        logger.write("No file '" + path.native() + "' found, it has been created with default parameters");
    }
    return settings;
}

struct RunParams {
    Optional<Float> targetRadius;
    Optional<Float> targetPeriod;
    Optional<Float> impactorRadius;
    Optional<Float> impactAngle;
    Optional<Float> impactSpeed;
    Optional<int> particleCnt;

    Optional<Float> stabTime;
    Optional<Float> fragTime;
    Optional<Float> reacTime;

    Optional<std::string> resumePath;
    Optional<std::string> outputPath;


    /// \brief Returns the name of the created output directory.
    std::string getOutputPath() const {
        if (outputPath) {
            return outputPath.value();
        }

        std::stringstream ss;
        ss << "sph_";
        if (targetRadius) {
            ss << round(targetRadius.value()) << "m_";
        }
        if (impactorRadius) {
            ss << round(impactorRadius.value()) << "m_";
        }
        if (targetPeriod) {
            ss << round(60._f * targetPeriod.value()) << "min_";
        }
        if (impactSpeed) {
            ss << round(impactSpeed.value() / 1.e3_f) << "kms_";
        }
        if (impactAngle) {
            ss << round(impactAngle.value()) << "deg_";
        }
        if (particleCnt) {
            ss << particleCnt.value() << "p_";
        }

        std::string name = ss.str();
        // drop the last "_";
        name.pop_back();
        return name;
    }
};

class RunFactory {
private:
    StringLogger logger;
    RunParams params;
    Path outputDir;
    bool doDryRun;
    std::string paramsMsg;

public:
    RunFactory(const RunParams& params)
        : params(params) {
        doDryRun = true;
        outputDir = Path(params.getOutputPath());
    }

    SharedPtr<WorkerNode> makeSimulation() {
        if (params.resumePath) {
            BinaryInput input;
            Expected<BinaryInput::Info> info = input.getInfo(Path(params.resumePath.value()));
            if (!info) {
                throw Exception("Cannot resume simulation from file '" + params.resumePath.value() + "'.\n" +
                                info.error());
            }

            logger.write("Resuming simulation from file '" + params.resumePath.value() + "'");
            if (info->runType == RunTypeEnum::SPH) {
                return resumeFragmentation();
            } else if (info->runType == RunTypeEnum::NBODY) {
                return resumeReaccumulation();
            } else {
                throw Exception("Cannot resume from this file.");
            }

        } else {
            logger.write("Starting new simulation");
            return makeNewSimulation();
        }
    }

    bool isDryRun() const {
        return doDryRun;
    }

    std::string getBannerMsg() const {
        return logger.toString() + "\n" + paramsMsg;
    }

    Path getOutputDir() const {
        return outputDir;
    }

private:
    Float defaultSphTime(const Optional<Float> runTime, const Optional<Float> radius, const Float mult) {
        // by default, use 1h for 100km body
        return runTime.valueOr(mult * 3600._f * radius.valueOr(5.e4_f) / 5.e4_f);
    }

    void overrideRunTime(RunSettings& settings, const Float endTime) {
        settings.set(RunSettingsId::RUN_END_TIME, endTime)
            .set(RunSettingsId::RUN_OUTPUT_INTERVAL, endTime / 10._f);
    }

    SharedPtr<WorkerNode> makeCollisionSetup() {
        // target IC
        BodySettings targetDefaults;
        targetDefaults.set(BodySettingsId::BODY_RADIUS, params.targetRadius.valueOr(50.e3_f))
            .set(BodySettingsId::PARTICLE_COUNT, params.particleCnt.valueOr(10000));
        if (params.targetPeriod) {
            targetDefaults.set(BodySettingsId::BODY_SPIN_RATE, 24._f / params.targetPeriod.value());
        }
        BodySettings targetBody =
            loadSettings<BodySettingsId>(outputDir / Path("target.cnf"), targetDefaults, logger, doDryRun);
        SharedPtr<WorkerNode> targetIc = makeNode<MonolithicBodyIc>("target body", targetBody);

        // impactor IC
        BodySettings impactorDefaults;
        impactorDefaults.set(BodySettingsId::BODY_RADIUS, params.targetRadius.valueOr(10.e3_f))
            .set(BodySettingsId::DAMAGE_MIN, LARGE)
            .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE);
        impactorDefaults.unset(BodySettingsId::PARTICLE_COUNT); // never used
        BodySettings impactorBody = loadSettings<BodySettingsId>(
            outputDir / Path("impactor.cnf"), impactorDefaults, logger, doDryRun);
        SharedPtr<WorkerNode> impactorIc = makeNode<ImpactorIc>("impactor body", impactorBody);
        targetIc->connect(impactorIc, "target");

        // target stabilization
        RunSettings stabDefaults = SphStabilizationWorker::getDefaultSettings("stabilization");
        stabDefaults.set(RunSettingsId::RUN_OUTPUT_PATH, outputDir.native());
        overrideRunTime(stabDefaults, defaultSphTime(params.stabTime, params.targetRadius, 0.2_f));

        RunSettings stabRun =
            loadSettings<RunSettingsId>(outputDir / Path("stab.cnf"), stabDefaults, logger, doDryRun);
        SharedPtr<WorkerNode> stabTarget = makeNode<SphStabilizationWorker>("stabilization", stabRun);
        targetIc->connect(stabTarget, "particles");

        // collision setup
        CollisionGeometrySettings geometryDefaults;
        geometryDefaults.set(CollisionGeometrySettingsId::IMPACT_SPEED, params.impactSpeed.valueOr(5.e3_f))
            .set(CollisionGeometrySettingsId::IMPACT_ANGLE, params.impactAngle.valueOr(45._f));
        CollisionGeometrySettings geometry = loadSettings<CollisionGeometrySettingsId>(
            outputDir / Path("geometry.cnf"), geometryDefaults, logger, doDryRun);
        SharedPtr<WorkerNode> setup = makeNode<CollisionGeometrySetup>("geometry", geometry);
        stabTarget->connect(setup, "target");
        impactorIc->connect(setup, "impactor");

        printRunSettings(targetBody, impactorBody, geometry);

        return setup;
    }

    SharedPtr<WorkerNode> makeFragmentation() {
        RunSettings fragDefaults = SphWorker::getDefaultSettings("fragmentation");
        fragDefaults.set(RunSettingsId::RUN_OUTPUT_PATH, outputDir.native());
        overrideRunTime(fragDefaults, defaultSphTime(params.fragTime, params.targetRadius, 1._f));
        RunSettings fragRun =
            loadSettings<RunSettingsId>(outputDir / Path("frag.cnf"), fragDefaults, logger, doDryRun);
        SharedPtr<WorkerNode> frag = makeNode<SphWorker>("fragmentation", fragRun);
        return frag;
    }

    SharedPtr<WorkerNode> makeReaccumulation() {
        RunSettings reacDefaults = NBodyWorker::getDefaultSettings("reaccumulation");
        reacDefaults.set(RunSettingsId::RUN_OUTPUT_PATH, outputDir.native());
        overrideRunTime(reacDefaults, params.reacTime.valueOr(3600._f * 24._f * 10._f));
        RunSettings reacRun =
            loadSettings<RunSettingsId>(outputDir / Path("reac.cnf"), reacDefaults, logger, doDryRun);
        SharedPtr<WorkerNode> reac = makeNode<NBodyWorker>("reaccumulation", reacRun);
        return reac;
    }

    SharedPtr<WorkerNode> makeNewSimulation() {
        SharedPtr<WorkerNode> setup = makeCollisionSetup();
        SharedPtr<WorkerNode> frag = makeFragmentation();
        setup->connect(frag, "particles");

        SharedPtr<WorkerNode> handoff = makeNode<SmoothedToSolidHandoff>("handoff"); // has no parameters
        frag->connect(handoff, "particles");

        SharedPtr<WorkerNode> reac = makeReaccumulation();
        handoff->connect(reac, "particles");
        return reac;
    }

    SharedPtr<WorkerNode> resumeFragmentation() {
        SharedPtr<WorkerNode> loadFile = makeNode<LoadFileWorker>(Path(params.resumePath.value()));

        SharedPtr<WorkerNode> frag = makeFragmentation();
        loadFile->connect(frag, "particles");

        SharedPtr<WorkerNode> handoff = makeNode<SmoothedToSolidHandoff>("handoff");
        frag->connect(handoff, "particles");

        SharedPtr<WorkerNode> reac = makeReaccumulation();
        handoff->connect(reac, "particles");
        return reac;
    }

    SharedPtr<WorkerNode> resumeReaccumulation() {
        SharedPtr<WorkerNode> loadFile = makeNode<LoadFileWorker>(Path(params.resumePath.value()));

        SharedPtr<WorkerNode> reac = makeReaccumulation();
        loadFile->connect(reac, "particles");
        return reac;
    }

    void printRunSettings(const BodySettings& targetBody,
        const BodySettings& impactorBody,
        const CollisionGeometrySettings& geometry) {
        const Float targetRadius = targetBody.get<Float>(BodySettingsId::BODY_RADIUS);
        const Float impactorRadius = impactorBody.get<Float>(BodySettingsId::BODY_RADIUS);
        const Float impactSpeed = geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_SPEED);
        const Float impactAngle = geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_ANGLE);
        const Float spinRate = targetBody.get<Float>(BodySettingsId::BODY_SPIN_RATE);
        const Size particleCnt = targetBody.get<int>(BodySettingsId::PARTICLE_COUNT);
        const Float rho = targetBody.get<Float>(BodySettingsId::DENSITY);
        const Float Q_D = evalBenzAsphaugScalingLaw(2._f * targetRadius, rho);
        const Float impactEnergy = getImpactEnergy(targetRadius, impactorRadius, impactSpeed) / Q_D;

        StringLogger logger;
        logger.setScientific(false);
        logger.setPrecision(4);
        logger.write();
        logger.write("Run parameters");
        logger.write("-------------------------------------");
        logger.write("  Target radius (R_pb):     ", 1.e-3_f * targetRadius, " km");
        logger.write("  Impactor radius (r_imp):  ", 1.e-3_f * impactorRadius, " km");
        logger.write("  Impact speed (v_imp):     ", 1.e-3_f * impactSpeed, " km/s");
        logger.write("  Impact angle (phi_imp):   ", impactAngle, "°");
        logger.write("  Impact energy (Q/Q*_D):   ", impactEnergy);
        logger.write("  Target period (P_pb):     ", 24._f / spinRate, spinRate == 0._f ? "" : "h");
        logger.write("  Particle count (N):       ", particleCnt);
        logger.write("-------------------------------------");
        logger.write();
        logger.setScientific(true);
        logger.setPrecision(PRECISION);

        paramsMsg = logger.toString();
    }
};

static void run(const ArgParser& parser, ILogger& logger) {
    RunParams params;
    params.targetRadius = parser.tryGetArg<Float>("tr");
    params.targetPeriod = parser.tryGetArg<Float>("tp");
    params.impactSpeed = parser.tryGetArg<Float>("v");
    if (params.impactSpeed) {
        params.impactSpeed.value() *= 1.e3_f; // km/s -> m/s
    }
    params.impactAngle = parser.tryGetArg<Float>("phi");
    params.impactorRadius = parser.tryGetArg<Float>("ir");
    params.particleCnt = parser.tryGetArg<int>("n");

    if (Optional<Float> impactEnergy = parser.tryGetArg<Float>("q")) {
        // we have to specify also -tr and -v, as output directory is determined fom the computed
        // impactorRadius. We cannot use values loaded from config files, as it would create circular
        // dependency: we need impactor radius to get output path, we need output path to load config
        // files, we need config files to get impactor radius ...
        const Float density = BodySettings::getDefaults().get<Float>(BodySettingsId::DENSITY);
        const Optional<Float> targetRadius = parser.tryGetArg<Float>("tr");
        const Optional<Float> impactSpeed = parser.tryGetArg<Float>("v");
        if (!targetRadius || !impactSpeed) {
            throw ArgError(
                "To specify impact energy (-q), you also need to specify the target radius (-tr) and "
                "impact speed (-v)");
        }
        params.impactorRadius = getImpactorRadius(
            targetRadius.value(), impactSpeed.value() * 1.e3_f, impactEnergy.value(), density);
    }

    params.outputPath = parser.tryGetArg<std::string>("o");
    params.resumePath = parser.tryGetArg<std::string>("i");

    params.stabTime = parser.tryGetArg<Float>("st");
    params.fragTime = parser.tryGetArg<Float>("ft");
    params.reacTime = parser.tryGetArg<Float>("rt");

    RunFactory factory(params);
    SharedPtr<WorkerNode> node = factory.makeSimulation();

    printBanner(logger);
    if (!factory.isDryRun()) {
        logger.write(factory.getBannerMsg());

        NullWorkerCallbacks callbacks;
        node->run(EMPTY_SETTINGS, callbacks);
    } else {
        printNoConfigsMsg(logger, factory.getOutputDir());
    }
}

int main(int argc, char* argv[]) {
    StdOutLogger logger;

    try {
        ArgParser parser(params);
        parser.parse(argc, argv);

        run(parser, logger);

        return 0;

    } catch (HelpException& e) {
        printBanner(logger);
        logger.write(e.what());
        return 0;
    } catch (Exception& e) {
        logger.write("Run failed!\n", e.what());
        return -1;
    }
}
