#include "gui/objects/Movie.h"
#include "gui/MainLoop.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Element.h"
#include "objects/containers/StringUtils.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <condition_variable>
#include <wx/image.h>


NAMESPACE_SPH_BEGIN

Movie::Movie(const GuiSettings& settings,
    std::unique_ptr<Abstract::Renderer>&& renderer,
    const RenderParams& params,
    Array<std::unique_ptr<Abstract::Element>>&& elements)
    : renderer(std::move(renderer))
    , params(params)
    , elements(std::move(elements)) {
    enabled = settings.get<bool>(GuiSettingsId::IMAGES_SAVE);
    outputStep = settings.get<Float>(GuiSettingsId::IMAGES_TIMESTEP);
    const std::string directory = settings.get<std::string>(GuiSettingsId::IMAGES_PATH);
    const std::string name = settings.get<std::string>(GuiSettingsId::IMAGES_NAME);
    paths = OutputFile(directory + name);
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


void Movie::onTimeStep(const std::shared_ptr<Storage>& storage, Statistics& stats) {
    if (stats.get<Float>(StatisticsId::TOTAL_TIME) < nextOutput || !enabled) {
        return;
    }
    const std::string path = paths.getNextPath();
    for (auto& e : elements) {
        std::string actPath = replace(path, "%e", escapeElementName(e->name()));
        e->initialize(*storage, ElementSource::POINTER_TO_STORAGE);
        /// \todo how about the lifetime of stats? Should be probably shared_ptr as well ...
        auto functor = [this, actPath, storage, &e, &stats] {
            // if the callback gets executed, it means the object is still alive and it's save to touch
            // the e directly
            std::unique_lock<std::mutex> lock(waitMutex);
            ArrayView<const Vector> positions = storage->getValue<Vector>(QuantityId::POSITIONS);
            Bitmap bitmap = renderer->render(positions, *e, params, stats);
            bitmap.saveToFile(actPath);
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
