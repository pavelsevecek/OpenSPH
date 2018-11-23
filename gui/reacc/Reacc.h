#pragma once

/// \file NBody.h
/// \brief Gravitational and collisional solver of N bodies
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "io/FileSystem.h"
#include "physics/Constants.h"
#include "run/CompositeRun.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class Reaccumulation;
class Fragmentation;

namespace Presets {
    class Collision;
} // namespace Presets

class Stabilization : public IRunPhase {
private:
    RawPtr<Controller> controller;
    SharedPtr<Presets::Collision> data;

public:
    Function<void()> onSphFinished;

    Function<void()> onStabilizationFinished;

    Stabilization(RawPtr<Controller> newController);

    ~Stabilization();

    virtual void setUp() override;

    virtual AutoPtr<IRunPhase> getNextPhase() const override;

    virtual void handoff(Storage&& UNUSED(input)) override {
        STOP; // this is the first phase, no handoff needed
    }

protected:
    virtual void tearDown(const Statistics& stats) override;
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
    virtual void tearDown(const Statistics& stats) override;
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
        gui.set(GuiSettingsId::ORTHO_FOV, 3.5e5_f)
            .set(GuiSettingsId::ORTHO_VIEW_CENTER, /*Vector(0, 300, 0)) // */ 0.5_f * Vector(1024, 768, 0))
            .set(GuiSettingsId::VIEW_WIDTH, 1024)
            .set(GuiSettingsId::VIEW_HEIGHT, 768)
            .set(GuiSettingsId::VIEW_MAX_FRAMERATE, 100)
            .set(GuiSettingsId::IMAGES_WIDTH, 1024)
            .set(GuiSettingsId::IMAGES_HEIGHT, 768)
            .set(GuiSettingsId::WINDOW_WIDTH, 1334)
            .set(GuiSettingsId::WINDOW_HEIGHT, 768)
            .set(GuiSettingsId::PARTICLE_RADIUS, 0.25_f)
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
            .set(GuiSettingsId::RAYTRACE_ITERATION_LIMIT, 10)
            .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 4)
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
            .set(GuiSettingsId::PLOT_OVERPLOT_SFD,
                std::string("/home/pavel/projects/astro/asteroids/hygiea/main_belt_families_2018/10_Hygiea/"
                            "size_distribution/family.dat_hc"))
            .set(GuiSettingsId::PLOT_INTEGRALS,
                PlotEnum::CURRENT_SFD | PlotEnum::PREDICTED_SFD | PlotEnum::TOTAL_ENERGY |
                    PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY);
        /*| PlotEnum::TOTAL_ENERGY |
      PlotEnum::TOTAL_MOMENTUM | PlotEnum::TOTAL_ANGULAR_MOMENTUM |
      PlotEnum::SIZE_FREQUENCY_DISTRIBUTION | PlotEnum::SELECTED_PARTICLE);*/

        gui.saveToFile(Path("gui.sph"));
        controller = makeAuto<Controller>(gui);

        AutoPtr<Stabilization> phase1 = makeAuto<Stabilization>(controller.get());

        phase1->onStabilizationFinished = [gui, this] {
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
