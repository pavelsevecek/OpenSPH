#include "gui/collision/Collision.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "sph/initial/Initial.h"
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
class ImpactorLogFile : public Abstract::LogFile {
private:
    QuantityMeans stress;
    QuantityMeans dtStress;
    QuantityMeans pressure;
    QuantityMeans energy;
    QuantityMeans density;


public:
    ImpactorLogFile(Storage& storage, const Path& path)
        : Abstract::LogFile(makeAuto<FileLogger>(path))
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

class EnergyLogFile : public Abstract::LogFile {
private:
    TotalEnergy en;
    TotalKineticEnergy kinEn;
    TotalInternalEnergy intEn;

public:
    EnergyLogFile(const Path& path)
        : Abstract::LogFile(makeAuto<FileLogger>(path)) {}

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

class TimestepLogFile : public Abstract::LogFile {
public:
    TimestepLogFile(const Path& path)
        : Abstract::LogFile(makeAuto<FileLogger>(path)) {}

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

AsteroidCollision::AsteroidCollision(RawPtr<Controller>&& controller)
    : controller(std::move(controller)) {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 10._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.1_f)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
    settings.saveToFile(Path("code.sph"));
}

void AsteroidCollision::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Range(0._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 100'000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e5_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    bodySettings.saveToFile(Path("target.sph"));

    storage = makeShared<Storage>();
    InitialConditions conds(*storage, settings);

    StdOutLogger logger;
    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    conds.addBody(domain1, bodySettings);
    /// \todo save also problem-specific settings: position of impactor, radius, ...
    logger.write("Particles of target: ", storage->getParticleCnt());
    const Size n1 = storage->getParticleCnt();

    //    SphericalDomain domain2(Vector(4785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
    SphericalDomain domain2(Vector(5097.4509902022_f, 3726.8662269290_f, 0._f), 270.5847632732_f);

    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 100)
        .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
        .set(BodySettingsId::DAMAGE_MIN, LARGE);
    bodySettings.saveToFile(Path("impactor.sph"));
    conds.addBody(domain2, bodySettings).addVelocity(Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
    logger.write("Particles of projectile: ", storage->getParticleCnt() - n1);

    Path outputDir = Path("out") / Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
    AutoPtr<TextOutput> textOutput = makeAuto<TextOutput>(
        outputDir, settings.get<std::string>(RunSettingsId::RUN_NAME), TextOutput::Options::SCIENTIFIC);
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

    logFiles.push(makeAuto<EnergyLogFile>(Path("energy.txt")));
    logFiles.push(makeAuto<TimestepLogFile>(Path("timestep.txt")));

    callbacks = makeAuto<GuiCallbacks>(controller);
}

void AsteroidCollision::tearDown() {
    Profiler& profiler = Profiler::getInstance();
    profiler.printStatistics(*logger);

    PkdgravOutput pkdgravOutput(Path("pkdgrav_%d.out"), PkdgravParams{});
    Statistics stats; /// \todo stats should survive after the run
    pkdgravOutput.dump(*storage, stats);
}

NAMESPACE_SPH_END