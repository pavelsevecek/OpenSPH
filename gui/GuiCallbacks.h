#pragma once

#include "gui/Controller.h"
#include "gui/objects/Movie.h"
#include "run/RunCallbacks.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

class GuiCallbacks : public IRunCallbacks {
private:
    RawPtr<Controller> model;

public:
    GuiCallbacks(const RawPtr<Controller> model)
        : model(model) {}

    virtual void onTimeStep(const Storage& storage, Statistics& stats) override {
        model->onTimeStep(storage, stats);
    }

    virtual void onRunStart(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onRunEnd(const Storage& UNUSED(storage), Statistics& stats) override {
        stats.get<Float>(StatisticsId::RUN_TIME);
    }

    virtual bool shouldAbortRun() const override {
        return model->shouldAbortRun();
    }
};

NAMESPACE_SPH_END
