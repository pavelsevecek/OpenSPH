#pragma once

/// \file NBody.h
/// \brief Gravitational and collisional solver of N bodies
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "io/Path.h"
#include "physics/Constants.h"
#include "run/CompositeRun.h"
#include <functional>
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class Reaccumulation;

class Fragmentation : public IRunPhase {
private:
    RawPtr<Controller> controller;

public:
    std::function<void()> onRunFinished;

    Fragmentation(RawPtr<Controller> newController);

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

protected:
    virtual void handoff(const Storage& UNUSED(input)) override {
        STOP; // this is the first phase, no handoff needed
    }

    virtual void tearDown() override;
};


class Reaccumulation : public IRunPhase {
public:
    std::function<void(const Storage&)> onRunStarted;

    Reaccumulation();

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override {
        return nullptr;
    }

    virtual void handoff(const Storage& input) override;

protected:
    virtual void tearDown() override;
};

class App : public wxApp {
private:
    AutoPtr<Controller> controller;

public:
    App() = default;

    ~App() {
        controller->quit();
    }

private:
    virtual bool OnInit() override {
        Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

        GuiSettings gui;
        gui.set(GuiSettingsId::ORTHO_FOV, 3.e7_f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, 0.5_f * Vector(1024, 768, 0))
            .set(GuiSettingsId::RENDER_WIDTH, 1024)
            .set(GuiSettingsId::RENDER_HEIGHT, 768)
            .set(GuiSettingsId::VIEW_WIDTH, 1024)
            .set(GuiSettingsId::VIEW_HEIGHT, 768)
            .set(GuiSettingsId::IMAGES_WIDTH, 1024)
            .set(GuiSettingsId::IMAGES_HEIGHT, 768)
            .set(GuiSettingsId::WINDOW_WIDTH, 1334)
            .set(GuiSettingsId::WINDOW_HEIGHT, 768)
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.25_f)
            .set(GuiSettingsId::CAMERA, int(CameraEnum::ORTHO))
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
            .set(GuiSettingsId::IMAGES_TIMESTEP, 1.e2_f)
            .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1._f, 1.e4_f))
            .set(GuiSettingsId::PALETTE_ENERGY, Interval(1._f, 5.e6_f))
            .setFlags(GuiSettingsId::PLOT_INTEGRALS,
                PlotEnum::TOTAL_ANGULAR_MOMENTUM | PlotEnum::SIZE_FREQUENCY_DISTRIBUTION);

        controller = makeAuto<Controller>(gui);

        AutoPtr<Fragmentation> frag = makeAuto<Fragmentation>(controller.get());

        frag->onRunFinished = [gui, this] {
            executeOnMainThread([gui, this] {
                GuiSettings newGui = gui;
                newGui.set(GuiSettingsId::PARTICLE_RADIUS, 1._f)
                    .set(GuiSettingsId::ORTHO_FOV, 8e7_f)
                    .set(GuiSettingsId::IMAGES_TIMESTEP, 100._f)
                    .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1._f, 1.e4_f))
                    .set(GuiSettingsId::IMAGES_NAME, std::string("reac_%e_%d.png"));
                controller->setParams(newGui);
            });
        };

        AutoPtr<IRun> allRuns = makeAuto<CompositeRun>(std::move(frag));
        controller->start(std::move(allRuns));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
