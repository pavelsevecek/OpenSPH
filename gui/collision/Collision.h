#pragma once

/// \file Collision.h
/// \brief Asteroid collision problem setup
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "io/Path.h"
#include "run/IRun.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class AsteroidCollision : public IRun {
private:
    RawPtr<Controller> controller;

public:
    /// Path to the directory containing pkdgrav executable
    Path pkdgravDir{ "/home/pavel/projects/astro/sph/external/pkdgrav_run/" };

    /// Path to the directory where results are saved, generated in constructor.
    Path outputDir;

    /// Path to the parent directory of outputDir
    Path resultsDir{ "." }; /// home/pavel/projects/astro/sph/result/" };

    /// Path to the source code, used to get git commit hash
    Path sourceDir{ "/home/pavel/projects/astro/sph/src/" };

    AsteroidCollision();

    void setController(RawPtr<Controller> newController) {
        controller = newController;
    }

    virtual void setUp() override;


protected:
    virtual void tearDown() override;

    void setupOutput();

    Storage runPkdgrav();
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
        gui.set(GuiSettingsId::ORTHO_FOV, 7e3_f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, 0.5_f * Vector(800, 600, 0))
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.2_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 5.e2_f)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::IMAGES_SAVE, true)
            .set(GuiSettingsId::IMAGES_TIMESTEP, 0.1_f)
            .set(GuiSettingsId::RENDERER, int(RendererEnum::SURFACE))
            .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.3_f, -0.4_f, 0.4_f)))
            .set(GuiSettingsId::SURFACE_SUN_INTENSITY, 0.8_f)
            .set(GuiSettingsId::SURFACE_AMBIENT, 0.1_f)
            .set(GuiSettingsId::SURFACE_RESOLUTION, 150._f)
            .set(GuiSettingsId::SURFACE_LEVEL, 0.9_f)
            .set(GuiSettingsId::PALETTE_DIVV, Interval(-1.e-6_f, 1.e-6_f));
        //.set(GuiSettingsId::ORTHO_CUTOFF, 0._f);

        AutoPtr<AsteroidCollision> run = makeAuto<AsteroidCollision>();

        gui.set(GuiSettingsId::IMAGES_PATH, (run->outputDir / "imgs"_path).native());
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
