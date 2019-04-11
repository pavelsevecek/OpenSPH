#include "gui/launcherGui/LauncherGui.h"
#include "gui/GuiCallbacks.h"
#include "gui/Uvw.h"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "physics/Constants.h"
#include "run/Collision.h"
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

static GuiSettings getGuiSettings() {
    GuiSettings gui;
    gui.set(GuiSettingsId::ORTHO_FOV, 0._f)
        .set(GuiSettingsId::VIEW_WIDTH, 1024)
        .set(GuiSettingsId::VIEW_HEIGHT, 768)
        .set(GuiSettingsId::VIEW_MAX_FRAMERATE, 100)
        .set(GuiSettingsId::IMAGES_WIDTH, 1024)
        .set(GuiSettingsId::IMAGES_HEIGHT, 768)
        .set(GuiSettingsId::WINDOW_WIDTH, 1600)
        .set(GuiSettingsId::WINDOW_HEIGHT, 768)
        .set(GuiSettingsId::PARTICLE_RADIUS, 0.35_f)
        .set(GuiSettingsId::SURFACE_RESOLUTION, 1.e2_f)
        .set(GuiSettingsId::SURFACE_LEVEL, 0.13_f)
        .set(GuiSettingsId::SURFACE_AMBIENT, 0.1_f)
        .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.4f, -0.1f, 0.6f)))
        .set(GuiSettingsId::RAYTRACE_HDRI, std::string("/home/pavel/projects/astro/sph/external/hdri3.jpg"))
        .set(GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY,
            std::string("/home/pavel/projects/astro/sph/external/surface.jpg"))
        .set(GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY,
            std::string("/home/pavel/projects/astro/sph/external/surface2.jpg"))
        .set(GuiSettingsId::RAYTRACE_ITERATION_LIMIT, 10)
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 4)
        .set(GuiSettingsId::CAMERA, CameraEnum::ORTHO)
        .set(GuiSettingsId::CAMERA_CUTOFF, 0._f)
        .set(GuiSettingsId::CAMERA_POSITION, Vector(0._f, 0._f, -1.e6f))
        .set(GuiSettingsId::IMAGES_SAVE, false)
        .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
        .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"))
        .set(GuiSettingsId::IMAGES_TIMESTEP, 10._f)
        .set(GuiSettingsId::PLOT_INITIAL_PERIOD, 60._f)
        .set(GuiSettingsId::PLOT_OVERPLOT_SFD,
            std::string("/home/pavel/projects/astro/asteroids/hygiea/main_belt_families_2018/10_Hygiea/"
                        "size_distribution/family.dat_hc"))
        .set(GuiSettingsId::PLOT_INTEGRALS,
            PlotEnum::KINETIC_ENERGY | PlotEnum::TOTAL_ENERGY | PlotEnum::INTERNAL_ENERGY |
                PlotEnum::TOTAL_ANGULAR_MOMENTUM | PlotEnum::TOTAL_MOMENTUM);

    const Path path("gui.sph");
    if (FileSystem::pathExists(path)) {
        gui.loadFromFile(path);
        /// \todo more systematic solution
        Rgba color = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
        color.a() = 1.f;
        gui.set(GuiSettingsId::BACKGROUND_COLOR, color);
    } else {
        gui.saveToFile(path);
    }
    return gui;
}

bool App::OnInit() {
    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    GuiSettings gui = getGuiSettings();
    controller = makeAuto<Controller>(gui);

    CollisionParams cp;
    cp.additionalBodySetup = [](Storage& body) { setupUvws(body); };

    PhaseParams phaseParams;
    phaseParams.stab.range = Interval(0._f, 5000._f);
    phaseParams.frag.range = Interval(0._f, 200000000._f);
    phaseParams.reacc.range = Interval(0._f, 1.e10_f);

    SharedPtr<GuiCallbacks> callbacks = makeShared<GuiCallbacks>(*controller);

    AutoPtr<CollisionRun> collision;
    if (wxTheApp->argc > 1) {
        Path resumePath(std::string(wxTheApp->argv[1]));
        try {
            collision = makeAuto<CollisionRun>(resumePath, phaseParams, callbacks);
        } catch (std::exception& e) {
            wxMessageBox("Cannot load the run state file " + resumePath.native() + "\n\nError: " + e.what(),
                "Error",
                wxOK);
            return true;
        }
    } else {
        collision = makeAuto<CollisionRun>(cp, phaseParams, callbacks);
    }

    collision->setOnNextPhase([gui, cp, run = collision.get(), this](const IRunPhase& next) {
        GuiSettings newGui = gui;

        if (typeid(next) == typeid(ReaccumulationRunPhase)) {
            newGui.set(GuiSettingsId::PARTICLE_RADIUS, 1._f)
                .set(GuiSettingsId::CAMERA_CUTOFF, 0._f)
                .set(GuiSettingsId::IMAGES_NAME, std::string("reac_%e_%d.png"))
                .set(GuiSettingsId::PLOT_INITIAL_PERIOD, 1000._f)
                .set(GuiSettingsId::IMAGES_TIMESTEP, 30._f)
                .set(GuiSettingsId::IMAGES_SAVE, false);

            controller->setParams(newGui);
        }

        controller->update(*next.getStorage());
    });

    controller->start(std::move(collision));
    return true;
}

NAMESPACE_SPH_END
