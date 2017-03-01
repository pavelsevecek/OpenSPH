#pragma once

/// Asteroid collision problem setup
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "gui/Settings.h"
#include "problem/Problem.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "system/LogFile.h"
#include "system/Logger.h"
#include "system/Output.h"

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
    QuantityMeans gradv;

    QuantityMeans pressure;
    QuantityMeans energy;
    QuantityMeans density;


public:
    ImpactorLogFile(Storage& storage, const std::string& path)
        : Abstract::LogFile(std::make_shared<FileLogger>(path))
        , stress(makeTensorFunctor(storage.getValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS)), 1)
        , dtStress(makeTensorFunctor(storage.getDt<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS)), 1)
        , gradv(makeTensorFunctor(storage.getValue<Tensor>(QuantityIds::RHO_GRAD_V)), 1)
        , pressure(QuantityIds::PRESSURE, 1)
        , energy(QuantityIds::ENERGY, 1)
        , density(QuantityIds::DENSITY, 1) {}

    virtual void write(Storage& storage, const Statistics& stats) override {
        Means sm = stress.evaluate(storage);
        Means dsm = dtStress.evaluate(storage);
        /// \todo get rid of these spaces
        this->logger->write(stats.get<Float>(StatisticsIds::TOTAL_TIME),
            "   ",
            sm.average(),
            "   ",
            dsm.average(),
            "   ",
            gradv.evaluate(storage).average(),
            "   ",
            energy.evaluate(storage).average(),
            "   ",
            pressure.evaluate(storage).average(),
            "  ",
            density.evaluate(storage).average(),
            "  ",
            sm.min(),
            "  ",
            sm.max(),
            "  ",
            dsm.min(),
            "  ",
            dsm.max());
    }
};

class EnergyLogFile : public Abstract::LogFile {
private:
    TotalEnergy en;
    TotalKineticEnergy kinEn;
    TotalInternalEnergy intEn;

public:
    EnergyLogFile(const std::string& path)
        : Abstract::LogFile(std::make_shared<FileLogger>(path)) {}

    virtual void write(Storage& storage, const Statistics& stats) override {
        this->logger->write(stats.get<Float>(StatisticsIds::TOTAL_TIME),
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
    TimestepLogFile(const std::string& path)
        : Abstract::LogFile(std::make_shared<FileLogger>(path)) {}

    virtual void write(Storage& UNUSED(storage), const Statistics& stats) override {
        if (!stats.has(StatisticsIds::LIMITING_PARTICLE_IDX)) {
            return;
        }
        const Float t = stats.get<Float>(StatisticsIds::TOTAL_TIME);
        const Float dt = stats.get<Float>(StatisticsIds::TIMESTEP_VALUE);
        const QuantityIds id = stats.get<QuantityIds>(StatisticsIds::LIMITING_QUANTITY);
        const int idx = stats.get<int>(StatisticsIds::LIMITING_PARTICLE_IDX);
        const Value value = stats.get<Value>(StatisticsIds::LIMITING_VALUE);
        const Value derivative = stats.get<Value>(StatisticsIds::LIMITING_DERIVATIVE);
        this->logger->write(t, " ", dt, " ", id, " ", idx, " ", value, " ", derivative);
    }
};

class Stress1456 : public Abstract::LogFile {
    FileLogger stepLogger;

public:
    Stress1456(const std::string& path)
        : Abstract::LogFile(std::make_shared<FileLogger>(path))
        , stepLogger("dt" + path) {}

    virtual void write(Storage& storage, const Statistics& stats) override {
        const Float t = stats.get<Float>(StatisticsIds::TOTAL_TIME);
        const Tensor smin(1.e5_f);
        ArrayView<TracelessTensor> s, ds;
        tie(s, ds) = storage.getAll<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        this->logger->write(t, " ", s[1456]);
        stepLogger.write(t, " ", 0.2_f * (abs(s[1456]) + smin) / abs(ds[1456]));
    }
};

/// \todo problems should be setup by inheriting Abstract::Problem. This interface should have something
/// like
/// setGlobalSettings, and setInitialConditions = 0.
struct AsteroidCollision {
    GlobalSettings globalSettings;

    void initialConditions(const std::shared_ptr<Storage>& storage) {
        BodySettings bodySettings;
        bodySettings.set(BodySettingsIds::ENERGY, 1._f)
            .set(BodySettingsIds::ENERGY_RANGE, Range(1._f, INFTY))
            .set(BodySettingsIds::PARTICLE_COUNT, 100000)
            .set(BodySettingsIds::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsIds::STRESS_TENSOR_MIN, 1.e7_f);
        bodySettings.saveToFile("target.sph");

        InitialConditions conds(storage, globalSettings);

        StdOutLogger logger;
        SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
        conds.addBody(domain1, bodySettings);
        /// \todo save also problem-specific settings: position of impactor, radius, ...
        logger.write("Particles of target: ", storage->getParticleCnt());
        const Size n1 = storage->getParticleCnt();

        //    SphericalDomain domain2(Vector(4785.5_f, 3639.1_f, 0._f), 146.43_f); // D = 280m
        SphericalDomain domain2(Vector(5097.45_f, 3726.87_f, 0._f), 270.585_f);

        bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100).set(BodySettingsIds::STRESS_TENSOR_MIN, LARGE);
        bodySettings.saveToFile("impactor.sph");
        conds.addBody(domain2, bodySettings, Vector(-5.e3_f, 0._f, 0._f)); // 5km/s
        logger.write("Particles of projectile: ", storage->getParticleCnt() - n1);
    }

    std::unique_ptr<Problem> getProblem() {
        globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
            .set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 5.e-3_f)
            .set(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
            .set(GlobalSettingsIds::RUN_OUTPUT_INTERVAL, 0.1_f)
            .set(GlobalSettingsIds::MODEL_FORCE_DIV_S, true)
            .set(GlobalSettingsIds::SPH_FINDER, FinderEnum::VOXEL)
            .set(GlobalSettingsIds::MODEL_AV_TYPE, ArtificialViscosityEnum::STANDARD)
            .set(GlobalSettingsIds::SPH_AV_ALPHA, 1.5_f)
            .set(GlobalSettingsIds::SPH_AV_BETA, 3._f)
            .set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
            .set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);
        globalSettings.saveToFile("code.sph");
        std::unique_ptr<Problem> p = std::make_unique<Problem>(globalSettings, std::make_shared<Storage>());
        std::string outputDir = "out/" + globalSettings.get<std::string>(GlobalSettingsIds::RUN_OUTPUT_NAME);
        p->output = std::make_unique<TextOutput>(outputDir,
            globalSettings.get<std::string>(GlobalSettingsIds::RUN_NAME),
            TextOutput::Options::SCIENTIFIC);
        p->output->add(std::make_unique<ParticleNumberColumn>());
        p->output->add(Factory::getValueColumn<Vector>(QuantityIds::POSITIONS));
        p->output->add(Factory::getDerivativeColumn<Vector>(QuantityIds::POSITIONS));
        p->output->add(Factory::getSmoothingLengthColumn());
        p->output->add(Factory::getValueColumn<Float>(QuantityIds::DENSITY));
        p->output->add(Factory::getValueColumn<Float>(QuantityIds::PRESSURE));
        p->output->add(Factory::getValueColumn<Float>(QuantityIds::ENERGY));
        p->output->add(Factory::getValueColumn<Float>(QuantityIds::DAMAGE));
        p->output->add(Factory::getValueColumn<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS));
        // Array<QuantityIds>{
        // QuantityIds::POSITIONS, QuantityIds::DENSITY, QuantityIds::PRESSURE, QuantityIds::ENERGY });
        //  QuantityIds::DAMAGE });
        // QuantityIds::DEVIATORIC_STRESS,
        // QuantityIds::RHO_GRAD_V });

        // ArrayView<Float> energy = storage->getValue<Float>(QuantityIds::ENERGY);

        initialConditions(p->storage);

        /* p->logs.push(std::make_unique<ImpactorLogFile>(*p->storage, "stress.txt"));*/
        p->logs.push(std::make_unique<EnergyLogFile>("energy.txt"));
        /*p->logs.push(std::make_unique<TimestepLogFile>("timestep.txt"));
        p->logs.push(std::make_unique<Stress1456>("s_1456.txt"));*/
        return p;
    }

    GuiSettings getGuiSettings() const {
        GuiSettings guiSettings;
        guiSettings.set(GuiSettingsIds::VIEW_FOV, 5.e3_f)
            .set(GuiSettingsIds::VIEW_CENTER, Vector(320, 200, 0._f))
            .set(GuiSettingsIds::PARTICLE_RADIUS, 0.3_f)
            .set(GuiSettingsIds::ORTHO_CUTOFF, 5.e2_f)
            .set(GuiSettingsIds::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsIds::IMAGES_SAVE, true)
            .set(GuiSettingsIds::IMAGES_TIMESTEP, 0.02_f);
        return guiSettings;
    }
};

NAMESPACE_SPH_END
