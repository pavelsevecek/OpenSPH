#include "gui/collision/Collision.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "sph/initial/Presets.h"
#include "sph/solvers/GravitySolver.h"
#include "system/Factory.h"
#include "system/Platform.h"
#include "system/Process.h"
#include "system/Profiler.h"
#include <fstream>
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

AsteroidCollision::AsteroidCollision() {

    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NEIGHBOUR_LIMIT, 10)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 100._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 10000._f))
        .setFlags(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE_GRADIENT | ForceEnum::SOLID_STRESS)
        // ForceEnum::GRAVITY) //| ForceEnum::INERTIAL)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_FORMULATION, FormulationEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::SPH_STABILIZATION_DAMPING, 0.1_f)
        .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));

    /*settings.set(RunSettingsId::RUN_COMMENT,
        std::string("Homogeneous Gravity with no initial rotation")); // + std::to_string(omega));

    // generate path of the output directory
    std::time_t t = std::time(nullptr);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%y-%m-%d_%H-%M");
    outputDir = resultsDir / Path(ss.str());
    settings.set(RunSettingsId::RUN_OUTPUT_PATH, (outputDir / "out"_path).native());

    // save code settings to the directory
    settings.saveToFile(outputDir / Path("code.sph"));

    // save the current git commit
    std::string gitSha = getGitCommit(sourceDir).value();
    FileLogger info(outputDir / Path("info.txt"));
    ss.str(""); // clear ss
    ss << std::put_time(std::localtime(&t), "%d.%m.%Y, %H:%M");
    info.write("Run ", runName);
    info.write("Started on ", ss.str());
    info.write("Git commit: ", gitSha);*/
}

void AsteroidCollision::setUp() {
    storage = makeShared<Storage>();


    if (wxTheApp->argc > 1) {
        std::string arg(wxTheApp->argv[1]);
        Path path(arg);
        if (!FileSystem::pathExists(path)) {
            wxMessageBox("Cannot locate file " + path.native(), "Error", wxOK);
            return;
        } else {
            BinaryOutput io;
            Statistics stats;
            Outcome result = io.load(path, *storage, stats);
            if (!result) {
                wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                return;
            } else {
                // const Float t0 = stats.get<Float>(StatisticsId::RUN_TIME);
                const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
                // const Interval origRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
                // settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(t0, origRange.upper()));
                settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);
            }
        }
    } else {
        Size N = 50000;

        BodySettings body;
        body.set(BodySettingsId::ENERGY, 0._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::DIEHL_ET_AL)
            .set(BodySettingsId::ENERGY_MIN, 10._f)
            .set(BodySettingsId::DAMAGE_MIN, 0.5_f);

        Presets::CollisionParams params;
        params.targetRadius = 1.e3_f;   // D = 2km
        params.impactorRadius = 1.e3_f; // D = 2km
        params.impactAngle = 45._f * DEG_TO_RAD;
        params.impactSpeed = 9._f; // v_imp = 5km/s
        params.targetRotation = 0._f;
        params.targetParticleCnt = N;
        params.centerOfMassFrame = true;
        params.optimizeImpactor = false;

        solver = Factory::getSolver(settings);
        Presets::Collision data(*solver, settings, body, params);
        data.addTarget(*storage);
        data.addImpactor(*storage);
    }

    callbacks = makeAuto<GuiCallbacks>(*controller);

    // add printing of run progres
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
}

void AsteroidCollision::tearDown() {

    /*Profiler& profiler = Profiler::getInstance();
    profiler.printStatistics(*logger);

    Storage output = this->runPkdgrav();
    if (output.getQuantityCnt() == 0) {
        return;
    }

    // get SFD from pkdgrav output
    Array<Post::SfdPoint> sfd = Post::getCummulativeSfd(output, {});
    FileLogger logSfd(outputDir / Path("sfd.txt"), FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logSfd.write(p.value, "  ", p.count);
    }

    // copy pkdgrav files (ss.[0-5][0-5]000) to output directory
    for (Path path : FileSystem::iterateDirectory(pkdgravDir)) {
        const std::string s = path.native();
        if (s.size() == 8 && s.substr(0, 3) == "ss." && s.substr(5) == "000") {
            FileSystem::copyFile(pkdgravDir / path, outputDir / "pkdgrav"_path / path);
        }
    }

    logger->write("FINISHED");*/
}

/*Storage AsteroidCollision::runPkdgrav() {
    ASSERT(FileSystem::pathExists(pkdgravDir));

    // change working directory to pkdgrav directory
    FileSystem::ScopedWorkingDirectory guard(pkdgravDir);

    // dump current particle storage using pkdgrav output
    PkdgravParams params;
    params.omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
    PkdgravOutput pkdgravOutput(Path("pkdgrav.out"), std::move(params));
    Statistics stats;
    pkdgravOutput.dump(*storage, stats);
    ASSERT(FileSystem::pathExists(Path("pkdgrav.out")));


    if (resultsDir == Path(".")) {
        // testing & debugging, do not run pkdgrav
        return {};
    }

    // convert the text file to pkdgrav input file and RUN PKDGRAV
    Process pkdgrav(Path("./RUNME.sh"), {});
    pkdgrav.wait();

    // convert the last output binary file to text file, using ss2bt binary
    ASSERT(FileSystem::pathExists(Path("ss.50000")));
    Process convert(Path("./ss2bt"), { "ss.50000" });
    convert.wait();
    ASSERT(FileSystem::pathExists(Path("ss.50000.bt")));

    // load the text file into storage
    Expected<Storage> output = Post::parsePkdgravOutput(Path("ss.50000.bt"));
    ASSERT(output);

    return std::move(output.value());
}
*/
NAMESPACE_SPH_END
