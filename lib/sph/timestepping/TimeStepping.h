#pragma once

/// Algorithms for temporal evolution of the physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "models/BasicModel.h"
#include "objects/containers/Array.h"
#include "sph/timestepping/Step.h"
#include "storage/Storage.h"
#include "system/Profiler.h"
#include <memory>

NAMESPACE_SPH_BEGIN


/// Base object providing integration in time for all quantities.
///
/// The integration is done by iterating with discrete time step, using step() method. All derived objects
/// must implement step() function, which iterates over all independant quantities and advances their values
/// using temporal derivatives computed by Abstract::Model passed in argument of the method.
///
/// The time-stepping object must take care of clearing derivatives, as there can be values from previous
/// timestep, or some garbage memory when the method is called for the first time. It is also necessary to
/// clamp all quantities by their minimal/maximal allowed values.
///
/// When model->compute is called, the storage passed as an argument MUST have zero highest order derivatives.
namespace Abstract {
    class TimeStepping : public Polymorphic {
    protected:
        std::shared_ptr<Storage> storage;
        Float dt;
        Float maxdt;
        Optional<TimeStepGetter> getter;

    public:
        TimeStepping(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings)
            : storage(storage) {
            dt    = settings.get<float>(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP).get();
            maxdt = settings.get<float>(GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP).get();
            if (settings.get<bool>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE).get()) {
                const Float factor =
                    settings.get<float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR).get();
                getter.emplace(storage, factor);
            }
        }

        INLINE Float getTimeStep() const { return dt; }

        void step(Abstract::Model* model) {
            this->stepImpl(model);
            // update time step
            if (getter) {
                this->dt = getter.get()(this->maxdt);
            }
        }

    protected:
        virtual void stepImpl(Abstract::Model* model) = 0;
    };
}


class EulerExplicit : public Abstract::TimeStepping {
public:
    explicit EulerExplicit(const std::shared_ptr<Storage>& storage,
                           const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {}

    virtual void stepImpl(Abstract::Model* model) override {
        // clear derivatives from previous timestep
        this->storage->init();

        // compute derivatives
        model->compute(*this->storage);

        PROFILE_SCOPE("EulerExplicit::step")
        // advance all 2nd-order quantities by current timestep, first values, then 1st derivatives
        iterate<TemporalEnum::SECOND_ORDER>(*this->storage, [this](auto& v, auto& dv, auto& d2v) {
            for (int i = 0; i < v.size(); ++i) {
                dv[i] += d2v[i] * this->dt;
                v[i] += dv[i] * this->dt;
                /// \todo clamp --- maybe use LimitedArray instead of Array in quantities? Better than passing range
                // as parameters, or by iterating twice
            }
        });
        iterate<TemporalEnum::FIRST_ORDER>(*this->storage, [this](auto& v, auto& dv) {
            for (int i = 0; i < v.size(); ++i) {
                v[i] += dv[i] * this->dt;
            }
        });
    }
};


class PredictorCorrector : public Abstract::TimeStepping {
private:
    Storage predictions;

public:
    explicit PredictorCorrector(const std::shared_ptr<Storage>& storage,
                                const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {
        ASSERT(storage->size() > 0); // quantities must already been emplaced
        predictions = storage->clone(TemporalEnum::HIGHEST_ORDER);
        storage->init(); // clear derivatives before using them in step method
    }

protected:
    virtual void stepImpl(Abstract::Model* model) override {
        const Float dt2 = 0.5_f * Math::sqr(this->dt);

        {
            PROFILE_SCOPE("PredictorCorrector::step   Predictions")
            // make prediction using old derivatives (simple euler)
            iterate<TemporalEnum::SECOND_ORDER>(*this->storage, [this, dt2](auto& v, auto& dv, auto& d2v) {
                for (int i = 0; i < v.size(); ++i) {
                    v[i] += dv[i] * this->dt + d2v[i] * dt2;
                    dv[i] += d2v[i] * this->dt;
                }
            });
            iterate<TemporalEnum::FIRST_ORDER>(*this->storage, [this](auto& v, auto& dv) {
                for (int i = 0; i < v.size(); ++i) {
                    v[i] += dv[i] * this->dt;
                }
            });
            // save derivatives from predictions
            this->storage->swap(predictions, TemporalEnum::HIGHEST_ORDER);

            // clear derivatives
            this->storage->init();
        }
        // compute model
        model->compute(*storage);

        PROFILE_SCOPE("PredictorCorrector::step   Corrections")
        // make corrections
        // clang-format off
        iteratePair<TemporalEnum::SECOND_ORDER>(*this->storage, this->predictions,
            [this, dt2](auto& pv, auto& pdv, auto& pd2v, auto& UNUSED(cv), auto& UNUSED(cdv), auto& cd2v) {
            ASSERT(pv.size() == pd2v.size());
            for (int i = 0; i < pv.size(); ++i) {
                pv[i] -= 0.333333_f * (cd2v[i] - pd2v[i]) * dt2;
                pdv[i] -= 0.5_f * (cd2v[i] - pd2v[i]) * this->dt;
            }
        });
        iteratePair<TemporalEnum::FIRST_ORDER>(*this->storage, this->predictions,
            [this](auto& pv, auto& pdv, auto& UNUSED(cv), auto& cdv) {
            ASSERT(pv.size() == pdv.size());
            for (int i = 0; i < pv.size(); ++i) {
                pv[i] -= 0.5_f * (cdv[i] - pdv[i]) * this->dt;
            }
        });
        // clang-format on
    }
};

class LeapFrog : public Abstract::TimeStepping {
public:
    LeapFrog(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(Abstract::Model* UNUSED(model)) override {}
};


class BulirschStoer : public Abstract::TimeStepping {
public:
    BulirschStoer(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings)
        : Abstract::TimeStepping(storage, settings) {}

protected:
    virtual void stepImpl(Abstract::Model* UNUSED(model)) override {}
};

NAMESPACE_SPH_END
