#include "gui/objects/Movie.h"
#include "gui/MainLoop.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"
#include "io/FileSystem.h"
#include "objects/containers/StringUtils.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <condition_variable>
#include <wx/image.h>


NAMESPACE_SPH_BEGIN

Movie::Movie(const GuiSettings& settings,
    AutoPtr<Abstract::Renderer>&& renderer,
    AutoPtr<Abstract::Camera>&& camera,
    Array<SharedPtr<Abstract::Element>>&& elements,
    const RenderParams& params)
    : renderer(std::move(renderer))
    , camera(std::move(camera))
    , elements(std::move(elements))
    , params(params) {
    enabled = settings.get<bool>(GuiSettingsId::IMAGES_SAVE);
    outputStep = settings.get<Float>(GuiSettingsId::IMAGES_TIMESTEP);
    const Path directory(settings.get<std::string>(GuiSettingsId::IMAGES_PATH));
    const Path name(settings.get<std::string>(GuiSettingsId::IMAGES_NAME));
    paths = OutputFile(directory / name);
    static bool first = true;
    if (first) {
        wxInitAllImageHandlers();
        first = false;
    }
    nextOutput = outputStep;
}

Movie::~Movie() = default;

INLINE std::string escapeElementName(const std::string& name) {
    std::string escaped = replace(name, " ", "");
    escaped = replace(escaped, ".", "_");
    return lowercase(escaped);
}


void Movie::onTimeStep(const Storage& storage, Statistics& stats) {
    if (stats.get<Float>(StatisticsId::TOTAL_TIME) < nextOutput || !enabled) {
        return;
    }
    const Path path = paths.getNextPath();
    createDirectory(path.parentPath());
    for (auto& e : elements) {
        Path actPath(replace(path.native(), "%e", escapeElementName(e->name())));
        e->initialize(storage, ElementSource::POINTER_TO_STORAGE);
        auto functor = [this, actPath, &storage, &e, &stats] {
            // if the callback gets executed, it means the object is still alive and it's save to touch
            // the e directly
            std::unique_lock<std::mutex> lock(waitMutex);
            ArrayView<const Vector> positions = storage.getValue<Vector>(QuantityId::POSITIONS);

            // initialize render with new data
            renderer->initialize(positions, *e, *camera);

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

void Movie::setEnabled(const bool enable) {
    enabled = enable;
}

NAMESPACE_SPH_END
