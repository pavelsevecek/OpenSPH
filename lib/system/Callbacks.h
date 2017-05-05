#pragma once

/// \file Callbacks.h
/// \brief Generic callbacks from the run, useful for GUI extensions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Statistics;
class Range;

namespace Abstract {
    class Callbacks : public Polymorphic {
    public:
        /// Called every timestep.
        virtual void onTimeStep(const SharedPtr<Storage>& storage, Statistics& stats) = 0;

        /// Called right before the run starts, i.e. after initial conditions are set up.
        virtual void onRunStart(const SharedPtr<Storage>& storage, Statistics& stats) = 0;

        /// Called after run ends. Does not get called if run is aborted.
        virtual void onRunEnd(const SharedPtr<Storage>& storage, Statistics& stats) = 0;

        /// Returns whether current run should be aborted or not. Can be called any time.
        virtual bool shouldAbortRun() const = 0;
    };
}

class NullCallbacks : public Abstract::Callbacks {
public:
    virtual void onTimeStep(const SharedPtr<Storage>&, Statistics&) override {}

    virtual void onRunStart(const SharedPtr<Storage>&, Statistics&) override {}

    virtual void onRunEnd(const SharedPtr<Storage>&, Statistics&) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

NAMESPACE_SPH_END
