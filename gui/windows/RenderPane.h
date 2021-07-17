#pragma once

#include "gui/objects/Bitmap.h"
#include "gui/renderers/IRenderer.h"
#include "run/Node.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class AnimationJob;
class IRenderPreview;

class BitmapOutput : public IRenderOutput {
private:
    wxPanel* panel;

    Bitmap<Rgba> render;
    std::mutex mutex;

public:
    explicit BitmapOutput(wxPanel* panel)
        : panel(panel) {}

    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) override;

    virtual void update(Bitmap<Rgba>&& bitmap, Array<Label>&& labels, const bool isFinal) override;

    wxPanel* getPanel() const {
        return panel;
    }

    wxBitmap getBitmap();
};

class InteractiveRenderer : public ShareFromThis<InteractiveRenderer> {
    SharedPtr<JobNode> node;
    RawPtr<AnimationJob> job;
    AutoPtr<IRenderPreview> preview;
    Pixel resolution;

    /// Holds the changes since the last \ref update.
    struct {
        /// \todo changed need to be protected by a mutex
        AutoPtr<ICamera> camera;
        Optional<RenderParams> parameters;
        AutoPtr<IColorizer> colorizer;
        AutoPtr<IRenderer> renderer;
        SharedPtr<JobNode> node;
        bool resolution = false;

        bool pending() const {
            return camera || parameters || colorizer || renderer || node || resolution;
        }
    } changed;

    struct Status {
        bool notInitialized = true;
        bool colorizerMissing = true;
        bool particlesMissing = true;
        bool cameraMissing = true;
        std::string otherReason;

        void clear() {
            notInitialized = colorizerMissing = particlesMissing = cameraMissing = false;
            otherReason.clear();
        }

        Outcome isValid() const;
    } status;

    /// \todo maybe replace std::thread with some IScheduler functionality
    std::thread thread;
    std::condition_variable cv;
    std::mutex mutex;

    std::atomic_bool quitting;

    BitmapOutput output;

public:
    InteractiveRenderer(const SharedPtr<JobNode>& node, wxPanel* panel);

    ~InteractiveRenderer();

    void start(const RunSettings& globals);

    wxBitmap getBitmap() {
        return output.getBitmap();
    }

    Outcome isValid() const {
        return status.isValid();
    }

    void resize(const Pixel newResolution);

private:
    AutoPtr<ICamera> getNewCamera(const SharedPtr<JobNode>& cameraNode, const RunSettings& globals) const;

    void setCameraAccessor(const RunSettings& globals, const SharedPtr<JobNode>& cameraNode);

    void setRendererAccessor(const RunSettings& globals);

    void setNodeAccessor(const SharedPtr<JobNode>& particleNode);

    void renderLoop(const RunSettings& globals);

    void update();

    void stop();
};

class RenderPane : public wxPanel {
private:
    SharedPtr<InteractiveRenderer> renderer;

public:
    RenderPane(wxWindow* parent,
        const wxSize size,
        const SharedPtr<JobNode>& node,
        const RunSettings& globals);

    ~RenderPane() = default;

private:
    void onPaint(wxPaintEvent& evt);
};

NAMESPACE_SPH_END
