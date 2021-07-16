#pragma once

#include "gui/objects/Bitmap.h"
#include "gui/objects/RenderJobs.h"
#include "quantities/Storage.h"
#include "run/Node.h"
#include "thread/CheckFunction.h"
#include <condition_variable>
#include <gui/MainLoop.h>
#include <mutex>
#include <thread>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class BitmapOutput : public IRenderOutput {
private:
    wxPanel* panel;

    Bitmap<Rgba> render;
    std::mutex mutex;

public:
    BitmapOutput(wxPanel* panel)
        : panel(panel) {}

    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) override {
        update(bitmap.clone(), std::move(labels), isFinal);
    }

    virtual void update(Bitmap<Rgba>&& bitmap,
        Array<Label>&& UNUSED(labels),
        const bool UNUSED(isFinal)) override {
        {
            std::unique_lock<std::mutex> lock(mutex);
            render = std::move(bitmap);
        }
        executeOnMainThread([this] {
            //
            //
            panel->Refresh();
        });
    }

    wxBitmap getBitmap() {
        std::unique_lock<std::mutex> lock(mutex);
        if (render.empty()) {
            return {};
        }

        wxBitmap bitmap;
        toWxBitmap(render, bitmap);
        return bitmap;
    }
};

class UpdateCameraCallbacks : public NullJobCallbacks {
private:
    RawPtr<const IJob> cameraJob = nullptr;
    AutoPtr<ICamera> camera;

public:
    virtual void onStart(const IJob& job) override {
        cameraJob = &job;
    }

    virtual void onEnd(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        SPH_ASSERT(cameraJob);
        SharedPtr<CameraData> data = cameraJob->getResult().getValue<CameraData>();
        camera = std::move(data->camera);
    }

    AutoPtr<ICamera> getCamera() const {
        return camera->clone();
    }
};


class RenderPane : public wxPanel {
    SharedPtr<JobNode> node;
    AutoPtr<IRenderPreview> preview;
    AutoPtr<ICamera> newCamera;

    /// \todo maybe replace std::thread with some IScheduler functionality
    std::thread thread;
    std::condition_variable cv;
    std::mutex mutex;

    std::atomic_bool quitting;

    BitmapOutput output;

public:
    RenderPane(wxWindow* parent,
        const wxSize size,
        const SharedPtr<JobNode>& node,
        const RunSettings& globals)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
        , node(node)
        , quitting(false)
        , output(this) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(RenderPane::onPaint));
        this->Bind(wxEVT_SIZE, [this](wxSizeEvent& UNUSED(evt)) { this->update(); });
        this->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& UNUSED(evt)) { this->stop(); });

        this->start(globals);
    }

    void start(const RunSettings& globals) {
        thread = std::thread([this, globals] {
            this->prepare(globals);
            if (preview) {
                this->renderLoop();
            }
        });
    }

    void prepare(const RunSettings& globals) {
        CHECK_FUNCTION(CheckFunction::NO_THROW);

        try {
            NullJobCallbacks callbacks;
            IJob& job = node->prepare(globals, callbacks);

            RawPtr<AnimationJob> animation = dynamicCast<AnimationJob, IJob>(&job);
            SPH_ASSERT(animation);
            preview = animation->getRenderPreview(globals);
        } catch (const InvalidSetup&) {
            preview = nullptr;
            return;
        }

        // find the camera among node providers
        SharedPtr<JobNode> cameraNode;
        SharedPtr<JobNode> particleNode;
        for (Size i = 0; i < node->getSlotCnt(); ++i) {
            const SlotData slot = node->getSlot(i);
            if (slot.type == GuiJobType::CAMERA) {
                cameraNode = slot.provider;
            } else if (slot.type == JobType::PARTICLES) {
                particleNode = slot.provider;
            }
        }
        SPH_ASSERT(cameraNode);   /// \todo throw if not found
        SPH_ASSERT(particleNode); /// \todo throw if not found
        cameraNode->setAccessor([this, globals, cameraNode](const JobNotificationType UNUSED(type)) {
            CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
            UpdateCameraCallbacks callbacks;
            cameraNode->run(globals, callbacks);
            newCamera = callbacks.getCamera();
            this->update();
        });

        /// \todo make sure the accessor expires when this closes
        node->setAccessor([this, globals](const JobNotificationType type) {
            CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
            this->stop();
            /*if (type == JobNotificationType::PROVIDER_CONNECTED) {
                this->start();
            }
            preview = animation->getRenderPreview(globals);*/
            this->start(globals);
        });

        particleNode->enumerate([this, globals](SharedPtr<JobNode> node, Size UNUSED(depth)) {
            node->setAccessor([this, node, globals](const JobNotificationType UNUSED(type)) {
                CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
                this->stop();
                this->start(globals);
            });
        });
    }

    void renderLoop() {
        // CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        quitting = false;
        // thread = std::thread([this] {
        CHECK_FUNCTION(CheckFunction::NO_THROW);
        while (!quitting) {
            std::unique_lock<std::mutex> lock(mutex);
            const wxSize size = this->GetClientSize();
            if (newCamera) {
                preview->update(std::move(newCamera));
            }

            preview->render(Pixel(size.x, size.y), output);
            cv.wait(lock);
        }
        //    });
    }

    void update() {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        if (!preview) {
            return;
        }

        preview->cancel();

        std::unique_lock<std::mutex> lock(mutex);
        cv.notify_one();
    }

    void stop() {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        quitting = true;
        if (preview) {
            preview->cancel();
            std::unique_lock<std::mutex> lock(mutex);
            cv.notify_one();
        }

        if (thread.joinable()) {
            thread.join();
        }
    }

    ~RenderPane() {
        this->stop();
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

        wxPaintDC dc(this);
        const wxSize size = dc.GetSize();
        wxBitmap bitmap = output.getBitmap();
        if (!bitmap.IsOk()) {
            dc.DrawText("Initializing ...", size.x / 2, size.y / 2);
            return;
        }
        const wxSize offset = size - bitmap.GetSize();
        dc.DrawBitmap(bitmap, wxPoint(offset.x / 2, offset.y / 2));
    }
};

NAMESPACE_SPH_END
