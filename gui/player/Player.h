#pragma once

/// \file Player.h
/// \brief Visualization of previously saved frames (as .ssf files)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "io/Output.h"
#include "run/IRun.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class RunPlayer : public IRun {
private:
    RawPtr<Controller> controller;

    OutputFile files;
    Size fileCnt;

    Float fps = 10._f;

    Float loadedTime;

public:
    RunPlayer();

    void setController(RawPtr<Controller> newController) {
        controller = newController;
    }

    virtual void setUp() override;

    virtual void run() override;


protected:
    virtual void tearDown(const Statistics& stats) override;

    Size getFileCount(const Path& pathMask) const;
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
        gui.set(GuiSettingsId::ORTHO_FOV, 3.5e3_f)
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
            .set(GuiSettingsId::SURFACE_LEVEL, 0.1_f)
            .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.2f, -0.1f, 1.1f)))
            .set(GuiSettingsId::SURFACE_RESOLUTION, 2.e3_f)
            .set(GuiSettingsId::CAMERA, CameraEnum::ORTHO)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::ORTHO_CUTOFF, 1.e2_f)
            .set(GuiSettingsId::IMAGES_SAVE, false)
            .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
            .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"))
            .set(GuiSettingsId::IMAGES_TIMESTEP, 10._f)
            .set(GuiSettingsId::PALETTE_STRESS, Interval(1.e5_f, 3.e6_f))
            .set(GuiSettingsId::PALETTE_VELOCITY, Interval(0.01_f, 1.e2_f))
            .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-5.e4_f, 5.e4_f))
            .set(GuiSettingsId::PALETTE_ENERGY, Interval(0._f, 1.e3_f))
            .set(GuiSettingsId::PALETTE_RADIUS, Interval(700._f, 3.e3_f))
            .set(GuiSettingsId::PALETTE_GRADV, Interval(0._f, 1.e-5_f))
            .set(GuiSettingsId::PLOT_INTEGRALS,
                PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_ENERGY |
                    PlotEnum::TOTAL_MOMENTUM | PlotEnum::TOTAL_ANGULAR_MOMENTUM |
                    PlotEnum::SELECTED_PARTICLE);

        controller = makeAuto<Controller>(gui);


        AutoPtr<RunPlayer> run = makeAuto<RunPlayer>();
        run->setController(controller.get());
        controller->start(std::move(run));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
