#pragma once

/// \file NBody.h
/// \brief Gravitational and collisional solver of N bodies
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Controller.h"
#include "gui/GuiCallbacks.h"
#include "gui/MainLoop.h"
#include "io/FileSystem.h"
#include "physics/Constants.h"
#include "run/RubblePile.h"

#include <wx/app.h>

NAMESPACE_SPH_BEGIN
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
        gui.set(GuiSettingsId::ORTHO_FOV, 1.5e5_f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, /*Vector(0, 300, 0)) // */ 0.5_f * Vector(1024, 768, 0))
            .set(GuiSettingsId::VIEW_WIDTH, 1024)
            .set(GuiSettingsId::VIEW_HEIGHT, 768)
            .set(GuiSettingsId::IMAGES_WIDTH, 1024)
            .set(GuiSettingsId::IMAGES_HEIGHT, 768)
            .set(GuiSettingsId::WINDOW_WIDTH, 1334)
            .set(GuiSettingsId::WINDOW_HEIGHT, 1030)
            .set(GuiSettingsId::PARTICLE_RADIUS, 1._f) // 0.25_f)
            .set(GuiSettingsId::SURFACE_RESOLUTION, 1.e2_f)
            .set(GuiSettingsId::SURFACE_LEVEL, 0.1_f)
            .set(GuiSettingsId::SURFACE_AMBIENT, 0.1_f)
            .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.4f, -0.1f, 0.6f)))
            .set(GuiSettingsId::RAYTRACE_HDRI,
                std::string("/home/pavel/projects/astro/sph/external/hdri3.jpg"))
            .set(GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY,
                std::string("/home/pavel/projects/astro/sph/external/surface.jpg"))
            .set(GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY,
                std::string("/home/pavel/projects/astro/sph/external/surface2.jpg"))
            .set(GuiSettingsId::CAMERA, CameraEnum::ORTHO)
            .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::ORTHO_ZOFFSET, -1.e8_f)
            .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(0._f, 0._f, -7.e3_f))
            .set(GuiSettingsId::IMAGES_SAVE, false)
            .set(GuiSettingsId::IMAGES_NAME, std::string("stab_%e_%d.png"))
            .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("stab_%e.avi"))
            .set(GuiSettingsId::IMAGES_TIMESTEP, 100._f)
            //.set(GuiSettingsId::IMAGES_RENDERER, int(RendererEnum::RAYTRACER))
            .set(GuiSettingsId::PALETTE_STRESS, Interval(1.e5_f, 3.e6_f))
            .set(GuiSettingsId::PALETTE_VELOCITY, Interval(0.01_f, 1.e2_f))
            .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-5.e4_f, 5.e4_f))
            .set(GuiSettingsId::PALETTE_ENERGY, Interval(1.e-1_f, 1.e3_f))
            .set(GuiSettingsId::PALETTE_RADIUS, Interval(700._f, 3.e3_f))
            .set(GuiSettingsId::PALETTE_GRADV, Interval(0._f, 1.e-5_f))
            .set(GuiSettingsId::PLOT_INITIAL_PERIOD, 10._f)
            .set(GuiSettingsId::PLOT_INTEGRALS, PlotEnum::ALL);

        controller = makeAuto<Controller>(gui);

        Presets::CollisionParams params;
        params.targetParticleCnt = 10000;
        params.targetRadius = 1.e5_f;
        params.impactorRadius = 2.e4_f;
        params.impactAngle = 0._f * DEG_TO_RAD;
        params.impactSpeed = 3.e3_f;
        params.impactorParticleCntOverride = 130;

        params.body.set(BodySettingsId::STRESS_TENSOR_MIN, 2.e8_f)
            .set(BodySettingsId::ENERGY_MIN, 100._f)
            .set(BodySettingsId::DAMAGE_MIN, 1._f)
            .set(BodySettingsId::MIN_PARTICLE_COUNT, 100)
            .set(BodySettingsId::BULK_POROSITY, 0.3_f);

        SharedPtr<GuiCallbacks> callbacks = makeShared<GuiCallbacks>(*controller);
        AutoPtr<RubblePileRunPhase> phase1 = makeAuto<RubblePileRunPhase>(params, callbacks);

        /*phase1->onStabilizationFinished = [gui, this] {
            executeOnMainThread([gui, this] {
                GuiSettings newGui = gui;
                newGui.set(GuiSettingsId::IMAGES_SAVE, false)
                    .set(GuiSettingsId::IMAGES_TIMESTEP, 50._f)
                    .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
                    //.set(GuiSettingsId::IMAGES_RENDERER, int(RendererEnum::RAYTRACER))
                    .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
                    .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"));
                controller->setParams(newGui);
            });
        };

        phase1->onSphFinished = [gui, this] {
            executeOnMainThread([gui, this] {
                GuiSettings newGui = gui;
                newGui
                    .set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f) // 1._f
                    .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1._f, 1.e4_f))
                    .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
                    .set(GuiSettingsId::IMAGES_NAME, std::string("reac_%e_%d.png"));
                controller->setParams(newGui);
            });
        };*/

        AutoPtr<CompositeRun> allRuns = makeAuto<CompositeRun>(std::move(phase1));
        allRuns->setPhaseCallback([gui, this](const Storage& storage) {
            executeOnMainThread([gui, this] {
                GuiSettings newGui = gui;
                newGui.set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f);
                controller->setParams(newGui);
            });
            controller->update(storage);
        });
        controller->start(std::move(allRuns));
        return true;
    }

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
