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
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class Reaccumulation;
class Fragmentation;

namespace Presets {
    class Collision;
}

class Stabilization : public IRunPhase {
private:
    RawPtr<Controller> controller;
    SharedPtr<Presets::Collision> data;

public:
    Function<void()> onSphFinished;

    Stabilization(RawPtr<Controller> newController);

    ~Stabilization();

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

    virtual void handoff(Storage&& UNUSED(input)) override {
        STOP; // this is the first phase, no handoff needed
    }

protected:
    virtual void tearDown() override;
};


class Fragmentation : public IRunPhase {
private:
    SharedPtr<Presets::Collision> data;

    Function<void()> onFinished;

public:
    Fragmentation(SharedPtr<Presets::Collision> data, Function<void()> onFinished);

    ~Fragmentation();

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

    virtual void handoff(Storage&& input) override;

protected:
    virtual void tearDown() override;
};


class Reaccumulation : public IRunPhase {
public:
    Reaccumulation();

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override {
        return nullptr;
    }

    virtual void handoff(Storage&& input) override;

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
        gui.set(GuiSettingsId::ORTHO_FOV, 80.e3_f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, /*Vector(0, 300, 0)) // */ 0.5_f * Vector(1024, 768, 0))
            .set(GuiSettingsId::RENDER_WIDTH, 1024)
            .set(GuiSettingsId::RENDER_HEIGHT, 768)
            .set(GuiSettingsId::VIEW_WIDTH, 1024)
            .set(GuiSettingsId::VIEW_HEIGHT, 768)
            .set(GuiSettingsId::IMAGES_WIDTH, 1024)
            .set(GuiSettingsId::IMAGES_HEIGHT, 768)
            .set(GuiSettingsId::WINDOW_WIDTH, 1334)
            .set(GuiSettingsId::WINDOW_HEIGHT, 768)
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.25_f)
            .set(GuiSettingsId::SURFACE_RESOLUTION, 2.e3_f)
            .set(GuiSettingsId::CAMERA, int(CameraEnum::ORTHO))
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::ORTHO_CUTOFF, 1500._f)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
            .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"))
            .set(GuiSettingsId::IMAGES_TIMESTEP, 10._f)
            .set(GuiSettingsId::PALETTE_STRESS, Interval(1.e5_f, 3.e6_f))
            .set(GuiSettingsId::PALETTE_VELOCITY, Interval(0.01_f, 1.e2_f))
            .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-5.e4_f, 5.e4_f))
            .set(GuiSettingsId::PALETTE_ENERGY, Interval(0._f, 1.e3_f))
            .set(GuiSettingsId::PALETTE_RADIUS, Interval(700._f, 3.e3_f))
            .set(GuiSettingsId::PALETTE_GRADV, Interval(0._f, 1.e-5_f))
            .setFlags(GuiSettingsId::PLOT_INTEGRALS,
                PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_ENERGY |
                    PlotEnum::TOTAL_MOMENTUM | PlotEnum::TOTAL_ANGULAR_MOMENTUM |
                    PlotEnum::SIZE_FREQUENCY_DISTRIBUTION | PlotEnum::SELECTED_PARTICLE);

        controller = makeAuto<Controller>(gui);

        AutoPtr<Stabilization> phase1 = makeAuto<Stabilization>(controller.get());

        phase1->onSphFinished = [gui, this] {
            executeOnMainThread([gui, this] {
                GuiSettings newGui = gui;
                newGui
                    .set(GuiSettingsId::PARTICLE_RADIUS, 1._f)
                    //.set(GuiSettingsId::ORTHO_FOV, 8e7_f)
                    .set(GuiSettingsId::IMAGES_TIMESTEP, 100._f)
                    .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1._f, 1.e4_f))
                    .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
                    .set(GuiSettingsId::IMAGES_NAME, std::string("reac_%e_%d.png"));
                controller->setParams(newGui);
            });
        };

        AutoPtr<CompositeRun> allRuns = makeAuto<CompositeRun>(std::move(phase1));
        allRuns->setPhaseCallback([this](const Storage& storage) { controller->update(storage); });
        controller->start(std::move(allRuns));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
