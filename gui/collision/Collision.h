#pragma once

/// \file Collision.h
/// \brief Asteroid collision problem setup
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "run/Run.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class AsteroidCollision : public Abstract::Run {
private:
    Controller* controller;

public:
    AsteroidCollision(Controller* controller);

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
        gui.set(GuiSettingsId::VIEW_FOV, 7e3_f)
            .set(GuiSettingsId::VIEW_CENTER, 0.5_f * Vector(1024, 768, 0))
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 5.e2_f)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_TIMESTEP, 0.01_f)
            .set(GuiSettingsId::IMAGES_RENDERER, int(RendererEnum::SURFACE))
            .set(GuiSettingsId::IMAGES_WIDTH, 1024)
            .set(GuiSettingsId::IMAGES_HEIGHT, 768)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::SURFACE_RESOLUTION, 70._f)
            .set(GuiSettingsId::SURFACE_LEVEL, 0.3_f);


        controller = makeAuto<Controller>(gui);

        AutoPtr<AsteroidCollision> run = makeAuto<AsteroidCollision>(controller.get());

        controller->start(std::move(run));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
