#include "gui/objects/Movie.h"
#include "gui/MainLoop.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "io/FileSystem.h"
#include "objects/utility/StringUtils.h"
#include "system/Process.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <condition_variable>
#include <wx/image.h>


NAMESPACE_SPH_BEGIN

Movie::Movie(const GuiSettings& settings,
    AutoPtr<IRenderer>&& renderer,
    AutoPtr<ICamera>&& camera,
    Array<SharedPtr<IColorizer>>&& colorizers,
    const RenderParams& params)
    : renderer(std::move(renderer))
    , camera(std::move(camera))
    , colorizers(std::move(colorizers))
    , params(params) {
    enabled = settings.get<bool>(GuiSettingsId::IMAGES_SAVE);
    outputStep = settings.get<Float>(GuiSettingsId::IMAGES_TIMESTEP);
    const Path directory(settings.get<std::string>(GuiSettingsId::IMAGES_PATH));
    const Path name(settings.get<std::string>(GuiSettingsId::IMAGES_NAME));
    paths = OutputFile(directory / name);

    const Path animationName(settings.get<std::string>(GuiSettingsId::IMAGES_MOVIE_NAME));
    animationPath = directory / animationName;

    static bool first = true;
    if (first) {
        wxInitAllImageHandlers();
        first = false;
    }
    nextOutput = outputStep;
}

Movie::~Movie() = default;

INLINE std::string escapeColorizerName(const std::string& name) {
    std::string escaped = replaceAll(name, " ", "");
    escaped = replaceAll(escaped, ".", "_");
    return lowercase(escaped);
}


void Movie::onTimeStep(const Storage& storage, Statistics& stats) {
    if (stats.get<Float>(StatisticsId::RUN_TIME) < nextOutput || !enabled) {
        return;
    }
    const Path path = paths.getNextPath(stats);
    FileSystem::createDirectory(path.parentPath());
    for (auto& e : colorizers) {
        Path actPath(replaceAll(path.native(), "%e", escapeColorizerName(e->name())));

        // initialize the colorizer
        e->initialize(storage, RefEnum::WEAK);

        // initialize render with new data (outside main thread)
        renderer->initialize(storage, *e, *camera);

        auto functor = [this, actPath, &storage, &e, &stats] {
            std::unique_lock<std::mutex> lock(waitMutex);

            // create the bitmap and save it to file
            SharedPtr<Bitmap> bitmap = renderer->render(*camera, params, stats);
            bitmap->saveToFile(actPath);

            waitVar.notify_one();
        };
        if (isMainThread()) {
            functor();
        } else {
            // wxWidgets don't like when we draw into DC from different thread
            std::unique_lock<std::mutex> lock(waitMutex);
            executeOnMainThread(functor);
            waitVar.wait(lock);
        }
    }
    nextOutput += outputStep;
}

void Movie::finalize() {
    for (auto& e : colorizers) {
        std::string name = escapeColorizerName(e->name());
        std::string outPath = animationPath.native();
        outPath = replaceAll(outPath, "%e", name);
        std::string inPath = paths.getMask().native();
        inPath = replaceAll(inPath, "%e", name);
        inPath = replaceAll(inPath, "%d", "%04d");
        // clang-format off
        Process ffmpeg(Path("/bin/ffmpeg"), {
                "-y",               // override existing files
                "-nostdin",         // don't prompt for confirmation, etc.
                "-framerate", "25", // 25 FPS
                "-i", inPath,
                "-c:v", "libx264",  // video codec
                outPath
            });
        // clang-format on
        ffmpeg.wait();
    }
}

void Movie::setEnabled(const bool enable) {
    enabled = enable;
}

NAMESPACE_SPH_END
