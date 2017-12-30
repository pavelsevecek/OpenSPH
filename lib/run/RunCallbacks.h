#pragma once

/// \file RunCallbacks.h
/// \brief Generic callbacks from the run, useful for GUI extensions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Statistics;
class Interval;

class IRunCallbacks : public Polymorphic {
public:
    /// Called every timestep.
    virtual void onTimeStep(const Storage& storage, Statistics& stats) = 0;

    /// Called right before the run starts, i.e. after initial conditions are set up.
    virtual void onRunStart(const Storage& storage, Statistics& stats) = 0;

    /// Called after run ends. Does not get called if run is aborted.
    virtual void onRunEnd(const Storage& storage, Statistics& stats) = 0;

    /// Returns whether current run should be aborted or not. Can be called any time.
    virtual bool shouldAbortRun() const = 0;
};

class NullCallbacks : public IRunCallbacks {
public:
    virtual void onTimeStep(const Storage&, Statistics&) override {}

    virtual void onRunStart(const Storage&, Statistics&) override {}

    virtual void onRunEnd(const Storage&, Statistics&) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

NAMESPACE_SPH_END