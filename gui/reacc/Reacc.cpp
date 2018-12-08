#include "gui/reacc/Reacc.h"
#include "gui/GuiCallbacks.h"
#include "io/Output.h"
#include "physics/Constants.h"
#include "run/Collision.h"
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

bool App::OnInit() {
    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    GuiSettings gui;
    gui.set(GuiSettingsId::ORTHO_FOV, 0._f)
        .set(GuiSettingsId::ORTHO_VIEW_CENTER, 0.5_f * Vector(1024, 768, 0))
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
        .set(GuiSettingsId::RAYTRACE_HDRI, std::string("/home/pavel/projects/astro/sph/external/hdri3.jpg"))
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
        .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
        .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"))
        .set(GuiSettingsId::IMAGES_TIMESTEP, 100._f)
        //.set(GuiSettingsId::IMAGES_RENDERER, int(RendererEnum::RAYTRACER))
        .set(GuiSettingsId::PALETTE_STRESS, Interval(1.e5_f, 3.e6_f))
        .set(GuiSettingsId::PALETTE_VELOCITY, Interval(0.01_f, 1.e2_f))
        .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-5.e4_f, 5.e4_f))
        .set(GuiSettingsId::PALETTE_ENERGY, Interval(1.e-1_f, 1.e3_f))
        .set(GuiSettingsId::PALETTE_RADIUS, Interval(700._f, 3.e3_f))
        .set(GuiSettingsId::PALETTE_GRADV, Interval(0._f, 1.e-5_f))
        .set(GuiSettingsId::PLOT_INITIAL_PERIOD, 1._f)
        .set(GuiSettingsId::PLOT_OVERPLOT_SFD,
            std::string("/home/pavel/projects/astro/asteroids/hygiea/main_belt_families_2018/10_Hygiea/"
                        "size_distribution/family.dat_hc"))
        .set(GuiSettingsId::PLOT_INTEGRALS,
            PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_ENERGY);

    controller = makeAuto<Controller>(gui);


    Presets::CollisionParams cp;
    cp.targetRadius = 100.e3_f; // 0.5_f * 550.e3_f;
    cp.impactAngle = 45._f * DEG_TO_RAD;
    cp.impactSpeed = 200._f; // 7.e3_f;
    cp.impactorRadius = 50.e3_f;
    cp.targetRotation = 0._f;
    cp.targetParticleCnt = 1000;
    cp.impactorOffset = 6;
    cp.centerOfMassFrame = false;
    cp.optimizeImpactor = false;
    PhaseParams phaseParams;
    phaseParams.stab.range = Interval(0._f, 100._f);
    phaseParams.frag.range = Interval(0._f, 200._f);
    phaseParams.reacc.range = Interval(0._f, 1.e10_f);


    SharedPtr<GuiCallbacks> callbacks = makeShared<GuiCallbacks>(*controller);

    AutoPtr<CollisionRun> collision;
    if (wxTheApp->argc > 1) {
        Path resumePath(std::string(wxTheApp->argv[1]));
        collision = makeAuto<CollisionRun>(resumePath, phaseParams, callbacks);
    } else {
        collision = makeAuto<CollisionRun>(cp, phaseParams, callbacks);
    }

    collision->setOnNextPhase([gui, this](const IRunPhase& next) {
        GuiSettings newGui = gui;

        if (typeid(next) == typeid(ReaccumulationRunPhase)) {
            newGui.set(GuiSettingsId::PARTICLE_RADIUS, 1._f)
                .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1._f, 1.e4_f))
                .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
                .set(GuiSettingsId::IMAGES_NAME, std::string("reac_%e_%d.png"));

            controller->setParams(newGui);
        }

        controller->update(*next.getStorage());
    });

    controller->start(std::move(collision));
    return true;
}

NAMESPACE_SPH_END
