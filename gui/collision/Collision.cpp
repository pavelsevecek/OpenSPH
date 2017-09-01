#include "gui/collision/Collision.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/geometry/Domain.h"
#include "post/Analysis.h"
#include "sph/equations/Potentials.h"
#include "sph/initial/Initial.h"
#include "sph/solvers/ContinuitySolver.h"
#include "sph/solvers/GravitySolver.h"
#include "system/Platform.h"
#include "system/Process.h"
#include "system/Profiler.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

template <typename TTensor>
class TensorFunctor {
private:
    ArrayView<TTensor> v;

public:
    TensorFunctor(ArrayView<TTensor> view) {
        v = view;
    }

    Float operator()(const Size i) {
        return sqrt(ddot(v[i], v[i]));
    }
};

template <typename TTensor>
TensorFunctor<TTensor> makeTensorFunctor(Array<TTensor>& view) {
    return TensorFunctor<TTensor>(view);
}

/// \todo we cache ArrayViews, this wont work if we will change number of particles during the run
class ImpactorLogFile : public ILogFile {
private:
    QuantityMeans stress;
    QuantityMeans dtStress;
    QuantityMeans pressure;
    QuantityMeans energy;
    QuantityMeans density;


public:
    ImpactorLogFile(Storage& storage, const Path& path)
        : ILogFile(makeAuto<FileLogger>(path))
        , stress(makeTensorFunctor(storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS)), 1)
        , dtStress(makeTensorFunctor(storage.getDt<TracelessTensor>(QuantityId::DEVIATORIC_STRESS)), 1)
        , pressure(QuantityId::PRESSURE, 1)
        , energy(QuantityId::ENERGY, 1)
        , density(QuantityId::DENSITY, 1) {}

protected:
    virtual void writeImpl(const Storage& storage, const Statistics& stats) override {
        MinMaxMean sm = stress.evaluate(storage);
        MinMaxMean dsm = dtStress.evaluate(storage);
        this->logger->write(stats.get<Float>(StatisticsId::TOTAL_TIME),
            sm.mean(),
            dsm.mean(),
            energy.evaluate(storage).mean(),
            pressure.evaluate(storage).mean(),
            density.evaluate(storage).mean(),
            sm.min(),
            sm.max(),
            dsm.min(),
            dsm.max());
    }
};

class EnergyLogFile : public ILogFile {
private:
    TotalEnergy en;
    TotalKineticEnergy kinEn;
    TotalInternalEnergy intEn;

public:
    EnergyLogFile(const Path& path)
        : ILogFile(makeAuto<FileLogger>(path)) {}

protected:
    virtual void writeImpl(const Storage& storage, const Statistics& stats) override {
        this->logger->write(stats.get<Float>(StatisticsId::TOTAL_TIME),
            "   ",
            en.evaluate(storage),
            "   ",
            kinEn.evaluate(storage),
            "   ",
            intEn.evaluate(storage));
    }
};

class TimestepLogFile : public ILogFile {
public:
    TimestepLogFile(const Path& path)
        : ILogFile(makeAuto<FileLogger>(path)) {}

protected:
    virtual void writeImpl(const Storage& UNUSED(storage), const Statistics& stats) override {
        if (!stats.has(StatisticsId::LIMITING_PARTICLE_IDX)) {
            return;
        }
        const Float t = stats.get<Float>(StatisticsId::TOTAL_TIME);
        const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
        const QuantityId id = stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY);
        const int idx = stats.get<int>(StatisticsId::LIMITING_PARTICLE_IDX);
        const Value value = stats.get<Value>(StatisticsId::LIMITING_VALUE);
        const Value derivative = stats.get<Value>(StatisticsId::LIMITING_DERIVATIVE);
        this->logger->write(t, " ", dt, " ", id, " ", idx, " ", value, " ", derivative);
    }
};


class CollisionSolver : public GenericSolver {
private:
    /// Body settings for the impactor
    BodySettings body;

    /// Used to create the impactor
    SharedPtr<InitialConditions> conds;

    /// Angular frequency
    Vector omega;

    /// Time range of the simulation.
    Interval timeRange;

    /// Time when initial dampening phase is ended and impact starts
    Float startTime = 0._f;

    /// Velocity damping constant
    Float delta = 0.1_f;

    /// Denotes the phase of the simulation
    bool impactStarted = false;

    /// Path where are results are saved
    Path outputPath;

public:
    CollisionSolver(const RunSettings& settings, const BodySettings& body, const Path& path)
        : GenericSolver(settings, this->getEquations(settings)) // , Factory::getGravity(settings))
        , body(body)
        , outputPath(path) {
        timeRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
        omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
    }

    void setInitialConditions(const SharedPtr<InitialConditions>& initialConditions) {
        conds = initialConditions;
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        GenericSolver::integrate(storage, stats);

        const Float t = stats.get<Float>(StatisticsId::TOTAL_TIME);
        const Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, 0.01_f);
        if (t <= startTime) {
            // damp velocities
            ArrayView<Vector> r, v, dv;
            tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
            for (Size i = 0; i < v.size(); ++i) {
                // gradually decrease the delta
                const Float t0 = timeRange.lower();
                v[i] /= 1._f + lerp(delta * dt, 0._f, min((t - t0) / (startTime - t0), 1._f));
                ASSERT(isReal(v[i]));
            }
        }
        if (t > startTime && !impactStarted) {
            body.set(BodySettingsId::PARTICLE_COUNT, 100)
                .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
                .set(BodySettingsId::DAMAGE_MIN, LARGE);
            SphericalDomain domain2(Vector(5598.423798_f, 2839.8390977_f, 0._f), 679.678195_f);
            body.saveToFile(outputPath / Path("impactor.sph"));
            conds
                ->addBody(domain2, body)
                // velocity 5 km/s
                .addVelocity(Vector(-6.e3_f, 0._f, 0._f))
                // flies straight, i.e. add rotation in non-intertial frame
                .addRotation(-omega, BodyView::RotationOrigin::FRAME_ORIGIN);

            impactStarted = true;
        }
        // update the angle
        Float phi = stats.getOr<Float>(StatisticsId::FRAME_ANGLE, 0._f);
        phi += getLength(omega) * dt;
        stats.set(StatisticsId::FRAME_ANGLE, phi);
    }

private:
    EquationHolder getEquations(const RunSettings& settings) const {
        // here we cannot use member variables as they haven't been initialized yet

        EquationHolder equations;

        // forces
        equations += makeTerm<PressureForce>() + makeTerm<SolidStressForce>(settings);

        // noninertial acceleration
        const Vector omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        equations += makeTerm<NoninertialForce>(omega);

        // gravity (approximation)
        equations += makeTerm<SphericalGravity>(SphericalGravity::Options::ASSUME_HOMOGENEOUS);

        // density evolution
        equations += makeTerm<ContinuityEquation>(settings);

        // artificial viscosity
        equations += EquationHolder(Factory::getArtificialViscosity(settings));

        // adaptive smoothing length
        equations += makeTerm<AdaptiveSmoothingLength>(settings);

        return equations;
    }
};

AsteroidCollision::AsteroidCollision() {
    const std::string runName = "Impact";
    const Float omega = 0._f; // 2._f * PI / (2._f * 3600._f);

    settings.set(RunSettingsId::RUN_NAME, runName)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(-50._f, 10._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.1_f)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f) /// \todo exception when using gravity with continuity solver?
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.5_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
        .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f, 0._f, omega));

    settings.set(RunSettingsId::RUN_COMMENT,
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
    info.write("Git commit: ", gitSha);
}

class StatsLog : public CommonStatsLog {
public:
    StatsLog(AutoPtr<ILogger>&& logger)
        : CommonStatsLog(std::move(logger)) {}

    virtual void writeImpl(const Storage& storage, const Statistics& stats) {
        CommonStatsLog::writeImpl(storage, stats);
        const int approximatedNodes = stats.get<int>(StatisticsId::GRAVITY_NODES_APPROX);
        const int exactNodes = stats.get<int>(StatisticsId::GRAVITY_NODES_EXACT);
        const Float ratio = Float(approximatedNodes) / (exactNodes + approximatedNodes);
        const int percent = int(ratio * 100._f);
        logger->write(" - approximated nodes: ", approximatedNodes, " (", percent, " %)");
        logger->write(" - exact nodes: ", exactNodes, " (", 100 - percent, " %)");
    }
};

void AsteroidCollision::setUp() {
    BodySettings body;
    body.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 100'000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e5_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    body.saveToFile(outputDir / Path("target.sph"));

    storage = makeShared<Storage>();
    AutoPtr<CollisionSolver> collisionSolver = makeAuto<CollisionSolver>(settings, body, outputDir);
    // AutoPtr<ContinuitySolver> collisionSolver = makeAuto<ContinuitySolver>(settings);

    SharedPtr<InitialConditions> conds = makeShared<InitialConditions>(*storage, *collisionSolver, settings);
    collisionSolver->setInitialConditions(conds);
    solver = std::move(collisionSolver);

    StdOutLogger logger;
    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    conds->addBody(domain1, body);
    logger.write("Particles of target: ", storage->getParticleCnt());

    /* body.set(BodySettingsId::PARTICLE_COUNT, 100)
         .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
         .set(BodySettingsId::DAMAGE_MIN, LARGE);
     SphericalDomain domain2(Vector(5097.4509902022_f, 3726.8662269290_f, 0._f), 270.5847632732_f);
     body.saveToFile(outputPath / Path("impactor.sph"));
     conds->addBody(domain2, body).addVelocity(Vector(-5.e3_f, 0._f, 0._f));*/


    this->setupOutput();

    callbacks = makeAuto<GuiCallbacks>(controller);

    // add printing of run progres
    logFiles.push(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
}

void AsteroidCollision::setupOutput() {
    Path outputPath = Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_PATH)) /
                      Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
    const std::string name = settings.get<std::string>(RunSettingsId::RUN_NAME);
    AutoPtr<TextOutput> textOutput = makeAuto<TextOutput>(outputPath, name, TextOutput::Options::SCIENTIFIC);
    textOutput->add(makeAuto<ParticleNumberColumn>());
    textOutput->add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    textOutput->add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
    textOutput->add(makeAuto<SmoothingLengthColumn>());
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    textOutput->add(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));

    output = std::move(textOutput);

    logFiles.push(makeAuto<EnergyLogFile>(outputDir / Path("energy.txt")));
    logFiles.push(makeAuto<TimestepLogFile>(outputDir / Path("timestep.txt")));
}

void AsteroidCollision::tearDown() {

    Profiler& profiler = Profiler::getInstance();
    profiler.printStatistics(*logger);

    if (resultsDir == Path(".")) {
        // testing & debugging, do not run pkdgrav
        return;
    }

    Storage output = this->runPkdgrav();

    // get SFD from pkdgrav output
    Array<Post::SfdPoint> sfd = Post::getCummulativeSfd(output, {});
    FileLogger logSfd(outputDir / Path("sfd.txt"), FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logSfd.write(p.radius, "  ", p.count);
    }

    // copy pkdgrav files (ss.[0-5][0-5]000) to output directory
    for (Path path : FileSystem::iterateDirectory(pkdgravDir)) {
        const std::string s = path.native();
        if (s.size() == 8 && s.substr(0, 3) == "ss." && s.substr(5) == "000") {
            FileSystem::copyFile(pkdgravDir / path, outputDir / "pkdgrav"_path / path);
        }
    }

    logger->write("FINISHED");
}

Storage AsteroidCollision::runPkdgrav() {
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

NAMESPACE_SPH_END
