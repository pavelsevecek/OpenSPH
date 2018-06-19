#pragma once

/// \file CachedGravity.h
/// \brief Wrapper of other IGravity object that uses cached accelerations to speed-up the evaluation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gravity/IGravity.h"
#include "system/Settings.h"

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
    ///                recomputation period.
    explicit CachedGravity(const Float recomputationPeriod, AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity)) {
        period = recomputationPeriod;
        ASSERT(period > 0._f);
        ASSERT(this->gravity);
    }

    virtual void evalAll(ArrayView<Vector> dv, Statistics& stats) const override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        ASSERT(t >= t_last);
        if (dv.size() == cachedDv.size() && t - t_last < period) {
            std::copy(cachedDv.begin(), cachedDv.end(), dv.begin());
            stats.set(StatisticsId::GRAVITY_EVAL_TIME, 0);
        } else {
            gravity->evalAll(dv, stats);
            cachedDv.resize(dv.size());
            std::copy(dv.begin(), dv.end(), cachedDv.begin());
            t_last = t;
        }
    }

    virtual void evalAll(ThreadPool& pool, ArrayView<Vector> dv, Statistics& stats) const override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        ASSERT(t >= t_last);
        if (dv.size() == cachedDv.size() && t - t_last < recomputationPeriod) {
            std::copy(cachedDv.begin(), cachedDv.end(), dv.begin());
            stats.set(StatisticsId::GRAVITY_EVAL_TIME, 0);
        } else {
            gravity->valAll(pool, dv, stats);
            cachedDv.resize(dv.size());
            std::copy(dv.begin(), dv.end(), cachedDv.begin());
            t_last = t;
        }
    }
};

NAMESPACE_SPH_END
