#include "gui/collision/Collision.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/Uvw.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "objects/geometry/Domain.h"
#include "sph/initial/Presets.h"
#include "sph/solvers/GravitySolver.h"
#include "system/Factory.h"
#include "system/Platform.h"
#include "system/Process.h"
#include "system/Profiler.h"
#include "thread/Pool.h"
#include <fstream>
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

AsteroidCollision::AsteroidCollision() {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-8_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 10000._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 2.e-3_f))
        .set(RunSettingsId::SOLVER_FORCES,
            ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS) //| ForceEnum::GRAVITY) //| ForceEnum::INERTIAL)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_USE_STRESS, true)
        .set(RunSettingsId::SPH_AV_STRESS_FACTOR, 0.04_f)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 5._f)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, false)
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

class SubtractDomain : public IDomain {
private:
    IDomain& primary;
    IDomain& subtracted;

public:
    SubtractDomain(IDomain& primary, IDomain& subtracted)
        : primary(primary)
        , subtracted(subtracted) {}

    virtual Vector getCenter() const override {
        // It does not have to be the EXACT geometric center, so let's return the center of
        // the primary domain for simplicity
        return primary.getCenter();
    }

    virtual Box getBoundingBox() const override {
        // Subtracted domain can only shrink the bounding box, so it's OK to return the primary one
        return primary.getBoundingBox();
    }

    virtual Float getVolume() const override {
        return primary.getVolume() - subtracted.getVolume();
    }

    virtual bool contains(const Vector& v) const override {
        // The main part - vector is contained in the domain if it is contained in the primary domain
        // and NOT contained in the subtracted domain
        return primary.contains(v) && !subtracted.contains(v);
    }

    // Additional functions needed by some components of the code that utilize the IDomain interface (such as
    // boundary conditions); they are not needed in this example, so let's simply mark them as not
    // implemented.

    virtual void getSubset(ArrayView<const Vector>, Array<Size>&, const SubsetType) const override {
        NOT_IMPLEMENTED
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED
    }

    virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
        NOT_IMPLEMENTED
    }

    virtual void addGhosts(ArrayView<const Vector>, Array<Ghost>&, const Float, const Float) const override {
        NOT_IMPLEMENTED
    }
};

void AsteroidCollision::setUp() {
    storage = makeShared<Storage>();
    scheduler = ThreadPool::getGlobalInstance();

    solver = Factory::getSolver(*scheduler, settings);

    if (wxTheApp->argc > 1) {
        std::string arg(wxTheApp->argv[1]);
        Path path(arg);
        if (!FileSystem::pathExists(path)) {
            wxMessageBox("Cannot locate file " + path.native(), "Error", wxOK);
            return;
        } else {
            BinaryInput input;
            Statistics stats;
            Outcome result = input.load(path, *storage, stats);
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
        Size N = 150000;

        BodySettings body;
        body.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::MURNAGHAN)
            .set(BodySettingsId::SHEAR_MODULUS, 1.e9_f)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::ELASTIC)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::ENERGY_MIN, 100._f)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e6_f)
            .set(BodySettingsId::DAMAGE_MIN, 10._f)
            .set(BodySettingsId::PARTICLE_COUNT, int(N));

        InitialConditions ic(*scheduler, *solver, settings);

        CylindricalDomain outerRing(Vector(0._f), 0.04_f, 0.01_f, true);
        CylindricalDomain innerRing(Vector(0._f), 0.03_f, 0.01_f, true);
        SubtractDomain domain(outerRing, innerRing);
        ic.addMonolithicBody(*storage, domain, body)
            .displace(Vector(-0.042_f, 0._f, 0._f))
            .addVelocity(Vector(80._f, 0._f, 0._f));
        ic.addMonolithicBody(*storage, domain, body)
            .displace(Vector(0.042_f, 0._f, 0._f))
            .addVelocity(Vector(-80._f, 0._f, 0._f));

        /*Presets::CollisionParams params;
        params.targetRadius = 4e5_f;
        params.impactorRadius = 1e5_f;
        params.impactAngle = 30._f * DEG_TO_RAD;
        params.impactSpeed = 5.e3_f;
        params.targetRotation = 0._f;
        params.targetParticleCnt = N;
        params.centerOfMassFrame = true;
        params.optimizeImpactor = true;
        params.body = body;

        solver = Factory::getSolver(*scheduler, settings);
        Presets::Collision data(*scheduler, settings, params);
        data.addTarget(*storage);
        data.addImpactor(*storage);

        setupUvws(*storage);*/
    }

    callbacks = makeAuto<GuiCallbacks>(*controller);

    // add printing of run progres
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings), settings));
}

void AsteroidCollision::tearDown(const Statistics& UNUSED(stats)) {

    /*Profiler& profiler = Profiler::getInstance();
    profiler.printStatistics(*logger);

    Storage output = this->runPkdgrav();
    if (output.getQuantityCnt() == 0) {
        return;
    }

    // get SFD from pkdgrav output
    Array<Post::SfdPoint> sfd = Post::getCumulativeSfd(output, {});
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
