#pragma once

/// Algorithms for temporal evolution of the physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "models/BasicModel.h"
#include "storage/GenericStorage.h"
#include "objects/containers/Array.h"
#include <memory>

NAMESPACE_SPH_BEGIN


/// To use generic method for timestepping all quantities.
namespace Abstract {
    class TimeStepping : public Polymorphic {
    protected:
        std::shared_ptr<GenericStorage> storage;
        Float dt;
        Float maxdt;

    public:
        TimeStepping(const std::shared_ptr<GenericStorage>& storage,
                     const Settings<GlobalSettingsIds>& settings)
            : storage(storage) {
            dt    = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP).get();
            maxdt = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP).get();
        }

        INLINE Float getTimeStep() const { return dt; }

        virtual void step(Abstract::Model* model) = 0;
    };
}


class EulerExplicit : public Abstract::TimeStepping {
public:
    explicit EulerExplicit(const std::shared_ptr<GenericStorage>& storage,
                           const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {}

    virtual void step(Abstract::Model* model) override {
        // clear derivatives from previous timestep
        //this->storage->init();
        // compute derivatives
        model->compute(*this->storage);

        // advance all 2nd-order quantities by current timestep, first values, then 1st derivatives
        /*this->storage->iterate<StorageIterableType::SECOND_ORDER>([this](auto& v, auto& dv, auto& d2v){
            for (int i=0; i<v.size(); ++i) {
                dv[i] += d2v[i] * this->dt;
                v[i] += dv[i] * this->dt;
            }
        });
        this->storage->iterate<StorageIterableType::FIRST_ORDER>([this](auto& v, auto& dv){
            for (int i=0; i<v.size(); ++i) {
                v[i] += dv[i] * this->dt;
            }
        });*/
    }
};


class PredictorCorrector : public Abstract::TimeStepping {
private:
    std::unique_ptr<GenericStorage> corrections;

public:
    explicit PredictorCorrector(const std::shared_ptr<GenericStorage>& storage,
                                const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {
        //corrections = storage->clone();
    }

    virtual void step(Abstract::Model* model) override {
        // clear derivatives from corrections
      //  corrections->init();

        // make prediction using old derivatives (simple euler)
        //const Float dt2 = 0.5 * Math::sqr(dt);
        /*auto v2p        = storage->getVisitor2ndOrder();
        for (int i = 0; i < v2p.getCnt(); ++i) {
            v2p[i].values += v2p[i].firstDerivs * this->dt + v2p[i].secondDerivs * dt2;
            v2p[i].firstDerivs += v2p[i].secondDerivs * this->dt;
        }
        auto v1p = storage->getVisitor1stOrder();
        for (int i = 0; i < v1p.getCnt(); ++i) {
            v1p[i].values += v1p[i].derivs * this->dt;
        }

        // compute model
        model->compute(*corrections);

        // make corrections
        auto v2c = corrections->getVisitor2ndOrder();
        ASSERT(v2c.getCnt() == v2p.getCnt());
        for (int i = 0; i < v2p.getCnt(); ++i) {
            v2p[i].values += 0.333333_f * (v2c[i].secondDerivs - v2p[i].secondDerivs) * dt2;
            v2p[i].firstDerivs += 0.5_f * (v2c[i].secondDerivs - v2p[i].secondDerivs) * this->dt;
        }
        auto v1c = corrections->getVisitor1stOrder();
        ASSERT(v1c.getCnt() == v1p.getCnt());
        for (int i = 0; i < v1p.getCnt(); ++i) {
            v1p[i].values += 0.5_f * (v1c[i].derivs - v1p[i].derivs) * this->dt;
        }

        // save derivatives for next step
        for (int i = 0; i < v2p.getCnt(); ++i) {
            v2p[i].secondDerivs = v2c[i].secondDerivs;
        }
        for (int i = 0; i < v1p.getCnt(); ++i) {
            v1p[i].derivs = v2c[i].derivs;
        }*/
    }
};

class BulirschStoer : public Abstract::TimeStepping {
public:
    BulirschStoer(const std::shared_ptr<GenericStorage>& storage,
                  const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {}

    virtual void step(Abstract::Model* model) override {}
};

NAMESPACE_SPH_END
