#include "gui/objects/Movie.h"
#include "gui/MainLoop.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "io/FileSystem.h"
#include "objects/utility/StringUtils.h"
#include "system/Process.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <condition_variable>
#include <mutex>
#include <wx/dcmemory.h>
#include <wx/image.h>

NAMESPACE_SPH_BEGIN

std::once_flag initFlag;

Movie::Movie(const GuiSettings& settings,
    AutoPtr<IRenderer>&& renderer,
    Array<SharedPtr<IColorizer>>&& colorizers,
    RenderParams&& params)
    : renderer(std::move(renderer))
    , colorizers(std::move(colorizers))
    , params(std::move(params)) {
    enabled = settings.get<bool>(GuiSettingsId::IMAGES_SAVE);
    makeAnimation = settings.get<bool>(GuiSettingsId::IMAGES_MAKE_MOVIE);
    outputStep = settings.get<Float>(GuiSettingsId::IMAGES_TIMESTEP);
    cameraVelocity = settings.get<Vector>(GuiSettingsId::CAMERA_VELOCITY);
    cameraOrbit = settings.get<Float>(GuiSettingsId::CAMERA_ORBIT);

    const Path directory(settings.get<std::string>(GuiSettingsId::IMAGES_PATH));
    const Path name(settings.get<std::string>(GuiSettingsId::IMAGES_NAME));
    const Size firstIndex(settings.get<int>(GuiSettingsId::IMAGES_FIRST_INDEX));
    paths = OutputFile(directory / name, firstIndex);

    const Path animationName(settings.get<std::string>(GuiSettingsId::IMAGES_MOVIE_NAME));
    animationPath = directory / animationName;

    std::call_once(initFlag, wxInitAllImageHandlers);
    nextOutput = outputStep;
}

Movie::~Movie() = default;

INLINE std::string escapeColorizerName(const std::string& name) {
    std::string escaped = replaceAll(name, " ", "");
    escaped = replaceAll(escaped, ".", "_");
    return lowercase(escaped);
}

class MovieRenderOutput : public IRenderOutput {
private:
    std::condition_variable waitVar;
    std::mutex waitMutex;

    Path currentPath;


public:
    void setPath(const Path& path) {
        currentPath = path;
    }

    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) override {
        if (!isFinal) {
            // no need for save intermediate results on disk
            return;
        }

        if (isMainThread()) {
            this->updateMainThread(bitmap, std::move(labels));
        } else {
            std::unique_lock<std::mutex> lock(waitMutex);
            executeOnMainThread([this, &bitmap, &labels] {
                std::unique_lock<std::mutex> lock(waitMutex);
                this->updateMainThread(bitmap, std::move(labels));
                waitVar.notify_one();
            });
            waitVar.wait(lock);
        }
    }

private:
    void updateMainThread(const Bitmap<Rgba>& bitmap, Array<Label>&& labels) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        wxBitmap wx;
        toWxBitmap(bitmap, wx);
        wxMemoryDC dc(wx);
        printLabels(dc, labels);
        dc.SelectObject(wxNullBitmap);
        saveToFile(wx, currentPath);
    }
};

void Movie::onTimeStep(const Storage& storage, Statistics& stats) {
    if (!enabled || stats.get<Float>(StatisticsId::RUN_TIME) < nextOutput) {
        return;
    }

    this->save(storage, stats);
    nextOutput += outputStep;
}

void Movie::save(const Storage& storage, Statistics& stats) {
    const Float time = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);
    const Float dt = time - lastFrame;

    const Vector target = params.camera->getTarget();
    const Vector cameraPos = params.camera->getFrame().translation();
    Vector dir = cameraPos - target;
    dir[H] = 0._f;
    if (cameraOrbit != 0._f) {
        const Vector up = params.camera->getUpVector();
        AffineMatrix rotation = AffineMatrix::rotateAxis(up, cameraOrbit * dt);
        dir = rotation * dir;
    }

    // move the camera (shared for all colorizers)
    if (params.tracker != nullptr) {
        Vector trackedPos, trackedVel;
        tie(trackedPos, trackedVel) = params.tracker->getTrackedPoint(storage);
        params.camera->setPosition(trackedPos + dir);
        params.camera->setTarget(trackedPos);
    } else {
        params.camera->setTarget(target + dt * cameraVelocity);
        params.camera->setPosition(target + dir + dt * cameraVelocity);
    }
    lastFrame = time;

    const Path path = paths.getNextPath(stats);
    FileSystem::createDirectory(path.parentPath());

    MovieRenderOutput output;
    for (auto& e : colorizers) {
        Path actPath(replaceAll(path.native(), "%e", escapeColorizerName(e->name())));

        // initialize the colorizer
        e->initialize(storage, RefEnum::WEAK);

        // initialize render with new data (outside main thread)
        renderer->initialize(storage, *e, *params.camera);

        // create the bitmap and save it to file
        output.setPath(actPath);
        renderer->render(params, stats, output);
    }
}

void Movie::setCamera(AutoPtr<ICamera>&& camera) {
    params.camera = std::move(camera);
}

void Movie::finalize() {
    if (!makeAnimation) {
        return;
    }

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
