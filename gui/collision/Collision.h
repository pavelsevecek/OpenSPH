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
        gui.set(GuiSettingsId::VIEW_FOV, 5.e3_f)
            .set(GuiSettingsId::VIEW_CENTER, Vector(320, 200, 0._f))
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 5.e2_f)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_TIMESTEP, 0.1_f)
            /// \todo rotation specific
            .set(GuiSettingsId::PALETTE_ENERGY, Range(0.1_f, 10._f))
            .set(GuiSettingsId::PALETTE_PRESSURE, Range(-10._f, 1.e6_f));

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
