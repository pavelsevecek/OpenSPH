#pragma once

/// \file Rotation.h
/// \brief Asteroid rotation and rotational fission
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "quantities/Storage.h"
#include "run/IRun.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class EquationHolder;

class AsteroidRotation : public IRun {
private:
    RawPtr<Controller> model;
    Float period;

public:
    /// \param period Rotational period of asteroid in hours
    AsteroidRotation(const RawPtr<Controller> model, const Float period);

    virtual void setUp() override;

protected:
    virtual void tearDown() override;

private:
    void setInitialStressTensor(Storage& smaller, EquationHolder& equations);
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
        gui.set(GuiSettingsId::ORTHO_FOV, 1._f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, Vector(320, 240, 0._f))
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.2_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0.005_f)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_TIMESTEP, 0.0001_f)
            .set(GuiSettingsId::VIEW_GRID_SIZE, 0.25_f)
            .set(GuiSettingsId::PALETTE_ENERGY, Interval(1.e-6_f, 1.e-2_f))
            .set(GuiSettingsId::PALETTE_VELOCITY, Interval(0.1_f, 8._f * PI))
            .set(GuiSettingsId::PALETTE_DIVV, Interval(-1.e-2_f, 1.e-2_f))
            .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-1.e6_f, 1.e2_f))
            .set(GuiSettingsId::PALETTE_STRESS, Interval(1.e4f, 1.e6_f))
            .set(GuiSettingsId::PALETTE_DENSITY_PERTURB, Interval(-1.e-4_f, 1.e-4_f));

        controller = makeAuto<Controller>(gui);

        AutoPtr<AsteroidRotation> run = makeAuto<AsteroidRotation>(controller.get(), 6._f);

        controller->start(std::move(run));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
