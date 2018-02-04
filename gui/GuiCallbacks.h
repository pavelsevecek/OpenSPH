#pragma once

#include "gui/Controller.h"
#include "gui/objects/Movie.h"
#include "run/RunCallbacks.h"
#include "system/Statistics.h"
#include "system/Timer.h"

NAMESPACE_SPH_BEGIN

class GuiCallbacks : public IRunCallbacks {
private:
    /// \todo avoid the dependency loop - controller is the owner of GuiCallbacks!
    Controller& controller;

public:
    GuiCallbacks(Controller& controller)
        : controller(controller) {}

    virtual void onTimeStep(const Storage& storage, Statistics& stats) override {
        Timer timer;
        controller.onTimeStep(storage, stats);
        stats.set(StatisticsId::POSTPROCESS_EVAL_TIME, timer.elapsed(TimerUnit::MILLISECOND));
    }

    virtual void onRunStart(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onRunEnd(const Storage& UNUSED(storage), Statistics& stats) override {
        stats.get<Float>(StatisticsId::RUN_TIME);
    }

    virtual bool shouldAbortRun() const override {
        return controller.shouldAbortRun();
    }
};

NAMESPACE_SPH_END
