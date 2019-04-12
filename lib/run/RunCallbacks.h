#pragma once

/// \file RunCallbacks.h
/// \brief Generic callbacks from the run, useful for GUI extensions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Statistics;
class Interval;
struct DiagnosticsError;

/// \brief Callbacks executed by the simulation to provide feedback to the user.
///
/// All functions are called from the same thread that called \ref IRun::run.
class IRunCallbacks : public Polymorphic {
public:
    /// \brief Called right before the run starts, i.e. after initial conditions are set up.
    ///
    /// Since this call, the run can arbitrarily modify the storage, so it is only safe to access the
    /// quantities from \ref onTimeStep calls.
    virtual void onRunStart(const Storage& storage, Statistics& stats) = 0;

    /// \brief Called after run ends and the storage is finalized.
    ///
    /// This is called after \ref IRun::tearDown. Since this call, run no longer modifies the storage and it
    /// is therefore safe to access the storage from different thread.
    virtual void onRunEnd(const Storage& storage, Statistics& stats) = 0;

    /// \brief Called every timestep.
    ///
    /// This is a blocking call, run is paused until the function returns. This allows to safely access the
    /// storage and run statistics. Note that accessing the storage from different thread during run is
    /// generally unsafe, as the storage can be resized during the run.
    virtual void onTimeStep(const Storage& storage, Statistics& stats) = 0;

    /// \brief Called if one of the run diagnostics reports a problem.
    virtual void onRunFailure(const DiagnosticsError& error, const Statistics& stats) const = 0;

    /// \brief Returns whether current run should be aborted or not.
    ///
    /// Can be called any time.
    virtual bool shouldAbortRun() const = 0;
};

class NullCallbacks : public IRunCallbacks {
public:
    virtual void onRunStart(const Storage&, Statistics&) override {}

    virtual void onRunEnd(const Storage&, Statistics&) override {}

    virtual void onTimeStep(const Storage&, Statistics&) override {}

    virtual void onRunFailure(const DiagnosticsError& UNUSED(error),
        const Statistics& UNUSED(stats)) const override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

NAMESPACE_SPH_END
