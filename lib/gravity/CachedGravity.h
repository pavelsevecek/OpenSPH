#pragma once

/// \file CachedGravity.h
/// \brief Wrapper of other IGravity object that uses cached accelerations to speed-up the evaluation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gravity/IGravity.h"
#include "system/Settings.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// \brief Wrapper of other IGravity implementation that approximates the gravity using cached values.
///
/// Particle accelerations are recomputed with given period and used in time steps between the subsequent
/// recomputations instead of computing all accelerations every time step. This is mainly useful if particles
/// move only slightly in one time step. Gravity is recomputed unconditionally if the number of particles
/// change.
class CachedGravity : public IGravity {
private:
    AutoPtr<IGravity> gravity;
    Float period;

    mutable Array<Vector> cachedDv;
    mutable Float t_last = -INFTY;

public:
    /// \brief Creates the cached gravity.
    ///
    /// \param recomputationPeriod Period of gravity recomputation in simulation time.
    /// \param gravity Actual implementation that computes the gravitational accelerations (roughly) every
    ///                recomputation period. Cannot be nullptr.
    explicit CachedGravity(const Float recomputationPeriod, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity)) {
        period = recomputationPeriod;
        ASSERT(period > 0._f);
        ASSERT(this->gravity);
    }

    virtual void build(IScheduler& scheduler, const Storage& storage) override {
        // here we have no information about the time, so we must build the gravity every time step; that's
        // fine as long as building time is significantly lower than the evaluation time.
        gravity->build(scheduler, storage);
    }

    virtual void evalAll(IScheduler& scheduler, ArrayView<Vector> dv, Statistics& stats) const override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        ASSERT(t >= t_last);
        if (dv.size() == cachedDv.size() && t - t_last < period) {
            std::copy(cachedDv.begin(), cachedDv.end(), dv.begin());
            stats.set(StatisticsId::GRAVITY_EVAL_TIME, 0);
        } else {
            gravity->evalAll(scheduler, dv, stats);
            cachedDv.resize(dv.size());
            std::copy(dv.begin(), dv.end(), cachedDv.begin());
            t_last = t;
        }
    }

    virtual Vector eval(const Vector& r0, Statistics& stats) const override {
        // we could cache this as well, but the function is mainly used for testing and some utilities where
        // performance does not matter, so it's probably not worth it.
        return gravity->eval(r0, stats);
    }

    virtual Float evalEnergy(IScheduler& scheduler, Statistics& stats) const override {
        return gravity->evalEnergy(scheduler, stats);
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        return gravity->getFinder();
    }
};

NAMESPACE_SPH_END
