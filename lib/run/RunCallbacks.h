#pragma once

/// \file RunCallbacks.h
/// \brief Generic callbacks from the run, useful for GUI extensions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Statistics;
class Interval;
struct DiagnosticsError;

class IRunCallbacks : public Polymorphic {
public:
    /// \brief Called right before the run starts, i.e. after initial conditions are set up.
    virtual void onRunStart(const Storage& storage, Statistics& stats) = 0;

    /// \brief Called after run ends. Does not get called if run is aborted.
    virtual void onRunEnd(const Storage& storage, Statistics& stats) = 0;

    /// \brief Called every timestep.
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
