#pragma once

/// \file NBody.h
/// \brief Gravitational and collisional solver of N bodies
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "io/Path.h"
#include "physics/Constants.h"
#include "run/IRun.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class NBody : public IRun {
private:
    RawPtr<Controller> controller;

public:
    NBody();

    void setController(RawPtr<Controller> newController) {
        controller = newController;
    }

    virtual void setUp() override;


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
        gui.set(GuiSettingsId::ORTHO_FOV, 1.2e3_f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, 0.5_f * Vector(1024, 768, 0))
            .set(GuiSettingsId::RENDER_WIDTH, 1024)
            .set(GuiSettingsId::RENDER_HEIGHT, 768)
            .set(GuiSettingsId::VIEW_WIDTH, 1024)
            .set(GuiSettingsId::VIEW_HEIGHT, 768)
            .set(GuiSettingsId::IMAGES_WIDTH, 1024)
            .set(GuiSettingsId::IMAGES_HEIGHT, 768)
            .set(GuiSettingsId::WINDOW_WIDTH, 1334)
            .set(GuiSettingsId::WINDOW_HEIGHT, 768)
            .set(GuiSettingsId::PARTICLE_RADIUS, 1._f)
            .set(GuiSettingsId::CAMERA, int(CameraEnum::ORTHO))
            .set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(0._f))
            .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(Constants::au, 0._f, 0._f))
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_TIMESTEP, 1.e6_f)
            .set(GuiSettingsId::PLOT_INTEGRALS,
                int(IntegralEnum::TOTAL_MOMENTUM) | int(IntegralEnum::TOTAL_ANGULAR_MOMENTUM))
            .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1.e-4_f, 1.e-2_f));

        AutoPtr<NBody> run = makeAuto<NBody>();

        controller = makeAuto<Controller>(gui);

        /// \todo try to remove this circular dependency
        run->setController(controller.get());

        controller->start(std::move(run));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
