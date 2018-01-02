#pragma once

/// \file Trigger.h
/// \brief Triggers of auxiliary actions during the run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/wrappers/AutoPtr.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

enum class TriggerEnum {
    ONE_TIME,  ///< Execute the trigger only once
    REPEATING, ///< Execute the trigger every time step
};

/// \brief Interface for triggering generic actions during the run.
class ITrigger : public Polymorphic {
public:
    /// \brief Returns the type of the trigger
    virtual TriggerEnum type() const = 0;

    /// \brief Returns true if the trigger should be executed
    ///
    /// \note Function is not const, so that triggers can increment internal counters, save last time or other
    /// auxiliary statistics, etc.
    virtual bool condition(const Storage& storage, const Statistics& stats) = 0;

    /// \brief Action executed when the condition is fulfilled.
    ///
    /// \return Additional trigger executed after this one, or nullptr.
    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) = 0;
};

/// \brief Trigger executing given action every period
class PeriodicTrigger : public ITrigger {
private:
    Float _period;
    Float _lastAction;

public:
    /// \brief period Period in simulation time of triggered action.
    explicit PeriodicTrigger(const Float period, const Float startTime)
        : _period(period)
        , _lastAction(startTime - EPS) {}

    virtual TriggerEnum type() const override {
        return TriggerEnum::REPEATING;
    }

    virtual bool condition(const Storage& UNUSED(storage), const Statistics& stats) override {
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        if (t > _lastAction + _period) {
            _lastAction = t;
            return true;
        } else {
            return false;
        }
    }
};

NAMESPACE_SPH_END
