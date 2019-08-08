#include "SsfToPng.h"
#include "Sph.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "thread/Tbb.h"
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
        .set(GuiSettingsId::IMAGES_WIDTH, 800)
        .set(GuiSettingsId::IMAGES_HEIGHT, 800)
        .set(GuiSettingsId::WINDOW_WIDTH, 1334)
        .set(GuiSettingsId::WINDOW_HEIGHT, 768)
        .set(GuiSettingsId::PARTICLE_RADIUS, 0.35_f)
        .set(GuiSettingsId::SURFACE_RESOLUTION, 1.e2_f)
        .set(GuiSettingsId::SURFACE_LEVEL, 0.25_f)
        .set(GuiSettingsId::SURFACE_SUN_INTENSITY, 0.92_f)
        .set(GuiSettingsId::SURFACE_AMBIENT, 0.05_f)
        .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.4f, -0.1f, 0.6f)))
        .set(GuiSettingsId::RENDERER, RendererEnum::RAYTRACER)
        .set(GuiSettingsId::RAYTRACE_ITERATION_LIMIT, 3)
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 0)
        .set(GuiSettingsId::CAMERA, CameraEnum::PERSPECTIVE)
        .set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(-4.e4, -3.8e4_f, 0._f))
        .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(-4.e4, -3.8e4_f, 6.e5_f))
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
        .set(GuiSettingsId::ORTHO_ZOFFSET, -1.e8_f)
        .set(GuiSettingsId::IMAGES_SAVE, true)
        .set(GuiSettingsId::IMAGES_NAME, std::string("img_%e_%d.png"))
        .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("img_%e.avi"))
        .set(GuiSettingsId::IMAGES_TIMESTEP, 0._f);


#if 0

    AutoPtr<IRenderer> renderer = Factory::getRenderer(*Tbb::getGlobalInstance(), gui);

    Array<SharedPtr<IColorizer>> colorizers;
    colorizers.push(Factory::getColorizer(gui, ColorizerId(QuantityId::BEAUTY)));

    RenderParams params;
    params.size = Pixel(1024, 768);
    params.camera = Factory::getCamera(gui, params.size);

    Movie movie(gui, std::move(renderer), std::move(colorizers), std::move(params));

    const Float totalRotation = 4._f * PI;
    const Float step = 0.02_f;
    try {
        Storage storage;
        Statistics stats;
        BinaryInput input;
        std::string file(wxTheApp->argv[1]);
        Outcome result = input.load(Path(file), storage, stats);
        if (!result) {
            throw IoError(result.error());
        }

        const Vector target = gui.get<Vector>(GuiSettingsId::PERSPECTIVE_TARGET);
        const Float radius = getLength(gui.get<Vector>(GuiSettingsId::PERSPECTIVE_POSITION) - target);
        for (Float phi = 0._f; phi < totalRotation; phi += step) {
            const Vector position =
                target + radius * (cos(phi) * Vector(0._f, 0._f, 1._f) + sin(phi) * Vector(1._f, 0._f, 0._f));
            gui.set(GuiSettingsId::PERSPECTIVE_POSITION, position);

            movie.setCamera(Factory::getCamera(gui, params.size));
            movie.save(storage, stats);
            std::cout << int(phi * 100 / totalRotation) << std::endl;
        }
    } catch (std::exception& e) {
        std::cout << "Failed: " << e.what() << std::endl;
        return -1;
    }
#else

    gui.set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(0._f))
        .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(0._f, 0._f, -6.5e5_f))
        .set(GuiSettingsId::PERSPECTIVE_TRACKED_PARTICLE, 201717);

    const Path dir(std::string(wxTheApp->argv[1]));
    for (Path path : FileSystem::iterateDirectory(dir)) {
        if (!OutputFile::getDumpIdx(path)) {
            continue;
        }

        Path ssf = dir / path;
        Path png = Path(ssf).replaceExtension("png");

        std::cout << "Processing " << ssf.native() << std::endl;
        gui.set(GuiSettingsId::IMAGES_NAME, png.native());

        AutoPtr<IRenderer> renderer = Factory::getRenderer(*Tbb::getGlobalInstance(), gui);

        Array<SharedPtr<IColorizer>> colorizers;
        Project project;
        project.getGuiSettings() = gui;
        colorizers.push(Factory::getColorizer(project, ColorizerId(QuantityId::MASS)));

        RenderParams params;
        params.size = Pixel(800, 800);
        params.camera = Factory::getCamera(gui, params.size);

        Movie movie(gui, std::move(renderer), std::move(colorizers), std::move(params));

        try {
            Storage storage;
            Statistics stats;
            BinaryInput input;
            Outcome result = input.load(ssf, storage, stats);
            if (!result) {
                throw IoError(result.error());
            }

            movie.save(storage, stats);

        } catch (std::exception& e) {
            std::cout << "Failed: " << e.what() << std::endl;
            return -1;
        }
    }
#endif
    return 0;
}
