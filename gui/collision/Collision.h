#pragma once

/// \file Collision.h
/// \brief Asteroid collision problem setup
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

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
    virtual void tearDown(const Statistics& stats) override;
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
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, /*Vector(0, 300, 0)) // */ 0.5_f * Vector(1024, 768, 0))
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
            .set(GuiSettingsId::CAMERA, int(CameraEnum::ORTHO))
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
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
            .set(GuiSettingsId::PLOT_INTEGRALS,
                PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_ENERGY |
                    PlotEnum::TOTAL_MOMENTUM | PlotEnum::TOTAL_ANGULAR_MOMENTUM |
                    PlotEnum::SELECTED_PARTICLE);

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
