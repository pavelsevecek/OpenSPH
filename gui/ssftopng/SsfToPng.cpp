#include "SsfToPng.h"
#include "Sph.h"
#include "gui/Factory.h"
#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include <iostream>

IMPLEMENT_APP(SsfToPngApp);

using namespace Sph;

bool SsfToPngApp::OnInit() {
    if (wxTheApp->argc < 2) {
        std::cout << "Usage: ssftopng file.ssf" << std::endl;
        return 0;
    }

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
        .set(GuiSettingsId::PARTICLE_RADIUS, 0.35_f)
        .set(GuiSettingsId::SURFACE_RESOLUTION, 1.e2_f)
        .set(GuiSettingsId::SURFACE_LEVEL, 0.1_f)
        .set(GuiSettingsId::SURFACE_AMBIENT, 0.1_f)
        .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.4f, -0.1f, 0.6f)))
        .set(GuiSettingsId::RENDERER, RendererEnum::RAYTRACER)
        .set(GuiSettingsId::RAYTRACE_ITERATION_LIMIT, 10)
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 4)
        .set(GuiSettingsId::CAMERA, CameraEnum::ORTHO)
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
        .set(GuiSettingsId::ORTHO_ZOFFSET, -1.e8_f)
        .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(0._f, 0._f, -7.e3_f))
        .set(GuiSettingsId::IMAGES_SAVE, true)
        .set(GuiSettingsId::IMAGES_NAME, std::string("img_%e_%d.png"))
        .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("img_%e.avi"))
        .set(GuiSettingsId::IMAGES_TIMESTEP, 0._f);

    AutoPtr<IRenderer> renderer = Factory::getRenderer(*ThreadPool::getGlobalInstance(), gui);

    Array<SharedPtr<IColorizer>> colorizers;
    colorizers.push(Factory::getColorizer(gui, ColorizerId::BEAUTY));

    RenderParams params;
    params.size = Pixel(1024, 768);
    params.camera = Factory::getCamera(gui, params.size);

    Movie movie(gui, std::move(renderer), std::move(colorizers), std::move(params));

    try {
        Storage storage;
        Statistics stats;
        BinaryInput input;
        std::string file(wxTheApp->argv[1]);
        Outcome result = input.load(Path(file), storage, stats);
        if (!result) {
            throw IoError(result.error());
        }

        movie.save(storage, stats);
    } catch (std::exception& e) {
        std::cout << "Failed: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
