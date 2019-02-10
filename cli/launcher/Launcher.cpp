/// \brief Executable running a single impact simulation, using command-line parameters


#include "Sph.h"
#include "run/Collision.h"

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
    logger.write("*********************************** OpenSPH ***********************************");
    logger.write("*******************************************************************************");
    logger.write("");
    logger.write("No command-line arguments specified and no configuration files found. Program  ");
    logger.write("will generate default configuration files and save them to the current working ");
    logger.write("directory.");
    logger.write("");
    logger.write("To start a simulation, re-run this program; it will load the generated files. ");
    logger.write("You can also specify parameters of the simulation as command-line arguments.  ");
    logger.write("Note that these arguments will override parameters loaded from configuration  ");
    logger.write("files. For more information, execute the program with -h (or --help) argument.");
    logger.write("");
}


/// \todo move to settings?
template <typename TValue = Float>
static Optional<TValue> tryGet(const CollisionGeometrySettings& settings,
    const CollisionGeometrySettingsId idx) {
    if (settings.has(idx)) {
        return settings.get<TValue>(idx);
    } else {
        return NOTHING;
    }
}


/// \brief Returns the name of the created output directory.
static std::string getRunName(const CollisionGeometrySettings& settings) {
    std::stringstream ss;
    ss << "sph_";
    if (Optional<Float> targetRadius = tryGet(settings, CollisionGeometrySettingsId::TARGET_RADIUS)) {
        ss << round(targetRadius.value()) << "m_";
    }
    if (Optional<Float> impactorRadius = tryGet(settings, CollisionGeometrySettingsId::IMPACTOR_RADIUS)) {
        ss << round(impactorRadius.value()) << "m_";
    }
    if (Optional<Float> targetSpinRate = tryGet(settings, CollisionGeometrySettingsId::TARGET_SPIN_RATE)) {
        ss << round(60._f * 24._f / targetSpinRate.value()) << "min_";
    }
    if (Optional<Float> impactSpeed = tryGet(settings, CollisionGeometrySettingsId::IMPACT_SPEED)) {
        ss << round(impactSpeed.value() / 1.e3_f) << "kms_";
    }
    if (Optional<Float> impactAngle = tryGet(settings, CollisionGeometrySettingsId::IMPACT_ANGLE)) {
        ss << round(impactAngle.value()) << "deg_";
    }
    if (Optional<int> particleCnt =
            tryGet<int>(settings, CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT)) {
        ss << particleCnt.value() << "p_";
    }

    std::string name = ss.str();
    // drop the last "_";
    name.pop_back();
    return name;
}

static bool doDryRun(Path directory) {
    if (directory.empty()) {
        directory = Path(".");
    }
    if (FileSystem::pathExists(directory)) {
        const Expected<FileSystem::PathType> type = FileSystem::pathType(directory);
        if (!type || type.value() != FileSystem::PathType::DIRECTORY) {
            return true;
        }
        Array<Path> files = FileSystem::getFilesInDirectory(directory);
        const Size configCount = std::count_if(
            files.begin(), files.end(), [](const Path& path) { return path.extension() == Path("sph"); });
        return configCount == 0;
    } else {
        return true;
    }
}

int main(int argc, char* argv[]) {
    StdOutLogger logger;
    ArgParser parser(params);
    try {
        parser.parse(argc, argv);
    } catch (std::exception& e) {
        logger.write(e.what());
        return -1;
    }

    CollisionParams cp;
    PhaseParams pp;
    try {
        // set all parsed values as overrides; also make sure to properly convert all units!
        parser.tryStore(cp.geometry, "tr", CollisionGeometrySettingsId::TARGET_RADIUS);
        parser.tryStore(cp.geometry, "tp", CollisionGeometrySettingsId::TARGET_SPIN_RATE, [](Float value) {
            // convert from P[h] to f[1/day]
            return 24._f / value;
        });
        parser.tryStore(cp.geometry, "v", CollisionGeometrySettingsId::IMPACT_SPEED, [](Float value) {
            return value * 1.e3_f; // km/s -> m/s
        });
        parser.tryStore(cp.geometry, "phi", CollisionGeometrySettingsId::IMPACT_ANGLE);
        parser.tryStore(cp.geometry, "ir", CollisionGeometrySettingsId::IMPACTOR_RADIUS);

        // using specified impact energy, compute the necessary impact radius
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
            const Float impactorRadius = getImpactorRadius(
                targetRadius.value(), impactSpeed.value() * 1.e3_f, impactEnergy.value(), density);
            cp.geometry.set(CollisionGeometrySettingsId::IMPACTOR_RADIUS, impactorRadius);
        }

        parser.tryStore(cp.geometry, "n", CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT);

        if (Optional<std::string> outputPath = parser.tryGetArg<std::string>("o")) {
            cp.outputPath = Path(outputPath.value());
        } else {
            const std::string runName = getRunName(cp.geometry);
            if (runName != "sph") {
                // only use if some parameters are specified in the filename
                cp.outputPath = Path(runName);
            }
        }
        pp.outputPath = cp.outputPath;

        Path resumePath;
        if (Optional<std::string> inputPath = parser.tryGetArg<std::string>("i")) {
            resumePath = Path(inputPath.value());
        }

        cp.logger = makeAuto<StdOutLogger>();

        // by default, use 1h for 100km body
        const Float runTime = 3600._f * parser.tryGetArg<Float>("tr").valueOr(5.e4_f) / 5.e4_f;
        pp.stab.range = Interval(0._f, 0.2_f * runTime);
        pp.frag.range = Interval(0._f, runTime);
        pp.reacc.range = Interval(0._f, 3600._f * 24._f * 10._f); // 10 days
        if (Optional<Float> stabTime = parser.tryGetArg<Float>("st")) {
            pp.stab.overrides.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, stabTime.value()));
        }
        if (Optional<Float> fragTime = parser.tryGetArg<Float>("ft")) {
            pp.frag.overrides.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, fragTime.value()));
        }
        if (Optional<Float> reacTime = parser.tryGetArg<Float>("rt")) {
            pp.reacc.overrides.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, reacTime.value()));
        }

        // write parameters to the standard output
        /*logger.write("Target radius [m]:             ", cp.targetRadius);
        logger.write("Impactor radius [m]:           ", cp.impactorRadius);
        logger.write("Target period [h]:             ", 2._f * PI / (3600._f * cp.targetRotation));
        logger.write(
            "Critical period [h]:           ", 2._f * PI / (3600._f * computeCriticalFrequency(2700._f)));
        logger.write("Impact speed [km/s]:           ", cp.impactSpeed / 1000._f);
        logger.write("Impact angle [°]:              ", cp.impactAngle * RAD_TO_DEG);
        logger.write("Impact energy [Q/Q*_D]:        ", impactEnergy);*/

        if (parser.empty() && doDryRun(cp.outputPath)) {
            // first run with no pararameters - just output the config files
            pp.dryRun = true;
            printBanner(logger);
            logger.write("Saving configuration files:");
        } else if (resumePath.empty()) {
            logger.write("Starting new run ", cp.outputPath);
        } else {
            logger.write("Resuming run from file ", resumePath.native());
        }
        logger.write("");

        AutoPtr<CollisionRun> run;
        if (resumePath.empty()) {
            run = makeAuto<CollisionRun>(cp, pp, makeShared<NullCallbacks>());
        } else {
            run = makeAuto<CollisionRun>(resumePath, pp, makeShared<NullCallbacks>());
        }
        run->setUp();
        run->run();

    } catch (std::exception& e) {
        logger.write(e.what());
        return -1;
    }

    return 0;
}
