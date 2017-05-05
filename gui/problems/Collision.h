#pragma once

/// Asteroid collision problem setup
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "gui/windows/MainWindow.h"
#include "io/LogFile.h"
#include "quantities/Storage.h"
#include "run/Run.h"

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
    ImpactorLogFile(Storage& storage, const std::string& path)
        : Abstract::LogFile(makeShared<FileLogger>(path))
        , stress(makeTensorFunctor(storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS)), 1)
        , dtStress(makeTensorFunctor(storage.getDt<TracelessTensor>(QuantityId::DEVIATORIC_STRESS)), 1)
        , pressure(QuantityId::PRESSURE, 1)
        , energy(QuantityId::ENERGY, 1)
        , density(QuantityId::DENSITY, 1) {}

    virtual void write(Storage& storage, const Statistics& stats) override {
        Means sm = stress.evaluate(storage);
        Means dsm = dtStress.evaluate(storage);
        this->logger->write(stats.get<Float>(StatisticsId::TOTAL_TIME),
            sm.average(),
            dsm.average(),
            energy.evaluate(storage).average(),
            pressure.evaluate(storage).average(),
            density.evaluate(storage).average(),
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
    EnergyLogFile(const std::string& path)
        : Abstract::LogFile(makeShared<FileLogger>(path)) {}

    virtual void write(Storage& storage, const Statistics& stats) override {
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
    TimestepLogFile(const std::string& path)
        : Abstract::LogFile(makeShared<FileLogger>(path)) {}

    virtual void write(Storage& UNUSED(storage), const Statistics& stats) override {
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

class AsteroidCollision : public Abstract::Run {
private:
    Controller* model;

public:
    AsteroidCollision(Controller* model);

    virtual SharedPtr<Storage> setUp() override;

protected:
    virtual void tearDown() override {}
};

NAMESPACE_SPH_END
