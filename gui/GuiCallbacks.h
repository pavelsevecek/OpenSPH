#pragma once

#include "gui/Controller.h"
#include "gui/Gui.h"
#include "gui/Renderer.h"
#include "gui/objects/Movie.h"
#include "system/Callbacks.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

class GuiCallbacks : public Abstract::Callbacks {
private:
    Controller* model;

public:
    GuiCallbacks(Controller* model)
        : model(model) {}

    virtual void onTimeStep(const std::shared_ptr<Storage>& storage, Statistics& stats) override {
        model->onTimeStep(storage, stats);
    }

    virtual void onRunStart(const std::shared_ptr<Storage>& UNUSED(storage),
        Statistics& UNUSED(stats)) override {}

    virtual void onRunEnd(const std::shared_ptr<Storage>& UNUSED(storage), Statistics& stats) override {
        stats.get<Float>(StatisticsId::TOTAL_TIME);
    }

    virtual bool shouldAbortRun() const override {
        return model->shouldAbortRun();
    }
};

NAMESPACE_SPH_END
