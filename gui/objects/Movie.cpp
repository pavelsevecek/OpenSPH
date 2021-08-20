#include "gui/objects/Movie.h"
#include "gui/MainLoop.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "io/FileSystem.h"
#include "objects/utility/StringUtils.h"
#include "quantities/QuantityHelpers.h"
#include "quantities/Attractor.h"
#include "system/Process.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <condition_variable>
#include <mutex>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/image.h>

NAMESPACE_SPH_BEGIN

std::once_flag initFlag;

Movie::Movie(const GuiSettings& settings,
    AutoPtr<IRenderer>&& renderer,
    AutoPtr<IColorizer>&& colorizer,
    RenderParams&& params,
    const int interpolatedFrames,
    const OutputFile& paths)
    : renderer(std::move(renderer))
    , colorizer(std::move(colorizer))
    , params(std::move(params))
    , interpolatedFrames(interpolatedFrames)
    , paths(paths) {
    cameraVelocity = settings.get<Vector>(GuiSettingsId::CAMERA_VELOCITY);
    cameraOrbit = settings.get<Float>(GuiSettingsId::CAMERA_ORBIT);

    std::call_once(initFlag, [] { executeOnMainThread(wxInitAllImageHandlers); });
}

Movie::~Movie() = default;

std::string escapeColorizerName(const std::string& name) {
    std::string escaped = replaceAll(name, " ", "");
    escaped = replaceAll(escaped, ".", "_");
    return lowercase(escaped);
}

void saveRender(Bitmap<Rgba>&& bitmap, Array<IRenderOutput::Label>&& labels, const Path& path) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxBitmap wx;
    toWxBitmap(bitmap, wx);
    wxGCDC dc(wx);
    printLabels(dc, labels);
    saveToFile(wx, path);
}

class ForwardingOutput : public IRenderOutput {
private:
    IRenderOutput& output;

    struct {
        Bitmap<Rgba> bitmap;
        Array<Label> labels;
    } final;

public:
    explicit ForwardingOutput(IRenderOutput& output)
        : output(output) {}

    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) override {
        if (isFinal) {
            output.update(bitmap, labels.clone(), isFinal);
            final.bitmap = bitmap.clone();
            final.labels = std::move(labels);
        } else {
            output.update(bitmap, std::move(labels), isFinal);
        }
    }

    virtual void update(Bitmap<Rgba>&& bitmap, Array<Label>&& labels, const bool isFinal) override {
        if (isFinal) {
            output.update(bitmap, labels.clone(), isFinal);
            final.bitmap = std::move(bitmap);
            final.labels = std::move(labels);
        } else {
            output.update(std::move(bitmap), std::move(labels), isFinal);
        }
    }

    bool hasData() const {
        return !final.bitmap.empty();
    }

    Bitmap<Rgba>& getBitmap() {
        return final.bitmap;
    }
    Array<Label>& getLabels() {
        return final.labels;
    }
};

void Movie::render(Storage&& storage, Statistics&& stats, IRenderOutput& output) {
    const Float time = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);

    ForwardingOutput forwardingOutput(output);
    if (interpolatedFrames > 0 && !lastFrame.data.empty()) {
        if (storage.getParticleCnt() != lastFrame.data.getParticleCnt()) {
            throw DataException("Cannot interpolate frames with different numbers of particles");
        }

        for (int frame = 0; frame < interpolatedFrames; ++frame) {
            const Float rel = Float(frame + 1) / Float(interpolatedFrames + 1);
            const Float interpTime = lerp(lastFrame.time, time, rel);
            Storage interpData = interpolate(lastFrame.data, storage, rel);
            stats.set(StatisticsId::RUN_TIME, interpTime);
            this->renderImpl(interpData, stats, forwardingOutput);
        }
    }

    this->renderImpl(storage, stats, forwardingOutput);

    lastFrame.time = time;
    if (interpolatedFrames > 0) {
        // need to keep this frame in memory
        lastFrame.data = std::move(storage);
    }
}

void Movie::renderImpl(const Storage& storage, Statistics& stats, ForwardingOutput& output) {
    const Float time = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);
    this->updateCamera(storage, time);

    // initialize the colorizer
    colorizer->initialize(storage, RefEnum::WEAK);

    // initialize render with new data (outside main thread)
    renderer->initialize(storage, *colorizer, *params.camera);

    renderer->render(params, stats, output);

    const Path path = paths.getNextPath(stats);
    FileSystem::createDirectory(path.parentPath());
    Path actPath(replaceAll(path.native(), "%e", escapeColorizerName(colorizer->name())));

    if (output.hasData()) {
        executeOnMainThread([bitmap = std::move(output.getBitmap()),
                                labels = std::move(output.getLabels()),
                                actPath]() mutable { //
            saveRender(std::move(bitmap), std::move(labels), actPath);
        });
    }
}

void Movie::updateCamera(const Storage& storage, const Float time) {
    const Float dt = time - lastFrame.time;

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
}


template <typename TValue>
Array<TValue> interpolate(ArrayView<const TValue> v1, ArrayView<const TValue> v2, const Float t) {
    SPH_ASSERT(v1.size() == v2.size());
    Array<TValue> result(v1.size());
    for (Size i = 0; i < v1.size(); ++i) {
        result[i] = lerp(v1[i], v2[i], t);
    }
    return result;
}

struct InterpolateVisitor {
    template <typename TValue>
    void visit(const QuantityId id, const Quantity& q1, const Quantity& q2, const Float t, Storage& result) {
        Quantity& q = result.getQuantity(id);
        q.getValue<TValue>() = interpolate<TValue>(q1.getValue<TValue>(), q2.getValue<TValue>(), t);
        if (q1.getOrderEnum() != OrderEnum::ZERO) {
            /// todo interpolate second-order too?
            q.setOrder(OrderEnum::FIRST);
            q.getDt<TValue>() = interpolate<TValue>(q1.getDt<TValue>(), q2.getDt<TValue>(), t);
        }
    }
};

Storage interpolate(const Storage& frame1, const Storage& frame2, const Float t) {
    if (frame1.getQuantityCnt() != frame2.getQuantityCnt()) {
        throw InvalidSetup("Different number of quantities");
    }
    if (frame1.getAttractorCnt() != frame2.getAttractorCnt()) {
        throw InvalidSetup("Different number of attractors");
    }

    Storage result = frame1.clone(VisitorEnum::ALL_BUFFERS);
    for (ConstStorageElement el1 : frame1.getQuantities()) {
        const Quantity& q1 = el1.quantity;
        const Quantity& q2 = frame2.getQuantity(el1.id);
        InterpolateVisitor visitor;
        dispatch(q1.getValueEnum(), visitor, el1.id, q1, q2, t, result);
    }
    for (Size i = 0; i < frame1.getAttractorCnt(); ++i) {
        const Attractor& a1 = frame1.getAttractors()[i];
        const Attractor& a2 = frame2.getAttractors()[i];
        Attractor& a = result.getAttractors()[i];
        a.position = lerp(a1.position, a2.position, t);
        a.velocity = lerp(a1.velocity, a2.velocity, t);
        a.mass = lerp(a1.mass, a2.mass, t);
        a.radius = lerp(a1.radius, a2.radius, t);
    }
    return result;
}

NAMESPACE_SPH_END
