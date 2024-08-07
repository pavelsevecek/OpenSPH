#include "gui/windows/RenderPage.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "gui/jobs/RenderJobs.h"
#include "gui/windows/Widgets.h"
#include "run/Node.h"
#include "thread/CheckFunction.h"

#include <wx/dcbuffer.h>
#include <wx/msgdlg.h>

#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>
// needs to be included after framemanager
#include <wx/aui/dockart.h>
#include <wx/weakref.h>

#include <condition_variable>
#include <mutex>

NAMESPACE_SPH_BEGIN

class ImagePane : public wxPanel {
private:
    Bitmap<Rgba> bitmap;
    Array<IRenderOutput::Label> labels;
    std::mutex mutex;
    std::condition_variable cv;

    TransparencyPattern pattern;
    Optional<float> scale;

public:
    ImagePane(wxWindow* parent)
        : wxPanel(parent, wxID_ANY) {
        this->SetMinSize(wxSize(640, 480));
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(ImagePane::onPaint));
        this->SetBackgroundColour(wxColour(Rgba::gray(0.2f)));
        this->SetBackgroundStyle(wxBG_STYLE_PAINT);
    }

    void update(Bitmap<Rgba>&& newBitmap, Array<IRenderOutput::Label>&& newLabels) {
        std::unique_lock<std::mutex> lock(mutex);
        bitmap = std::move(newBitmap);
        labels = std::move(newLabels);

        executeOnMainThread([this] {
            if (!scale) {
                const wxSize windowSize = this->GetSize();
                // shrink the view if larger than the window
                scale =
                    max(float(bitmap.size().x) / windowSize.x, float(bitmap.size().y) / windowSize.y, 1.f);
            }
            this->Refresh();
            std::unique_lock<std::mutex> lock(mutex);
            cv.notify_one();
        });
        cv.wait(lock);
    }

    void zoom(const float amount) {
        if (!scale) {
            return;
        }
        scale.value() *= (amount > 0.f) ? 1.2f : 1.f / 1.2f;
        scale.value() = clamp(scale.value(), 0.25f, 4.f);
        this->Refresh();
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        wxAutoBufferedPaintDC dc(this);
        const wxSize size = this->GetClientSize();
        dc.Clear();

        if (bitmap.empty()) {
            return;
        }
        SPH_ASSERT(scale);

        wxBitmap wx;
        {
            std::unique_lock<std::mutex> lock(mutex);
            toWxBitmap(bitmap, wx, scale.value());
        }

        const wxSize diff = size - wx.GetSize();
        wxPoint offset(diff.x / 2, diff.y / 2);
        pattern.draw(dc, wxRect(offset, wx.GetSize()));

        dc.DrawBitmap(wx, offset);

        Array<IRenderOutput::Label> scaledLabels = labels.clone();
        for (IRenderOutput::Label& label : scaledLabels) {
            label.position = Pixel(Coords(label.position) / scale.value()) + Pixel(offset);
            label.fontSize /= scale.value();
        }
        printLabels(dc, scaledLabels);
    }
};

class RenderPageCallbacks : public IJobCallbacks {
private:
    RawPtr<ImagePane> pane;
    RawPtr<ProgressPanel> progress;
    std::atomic_bool cancelled;

public:
    RenderPageCallbacks(RawPtr<ImagePane> pane, RawPtr<ProgressPanel> progress)
        : pane(pane)
        , progress(progress) {
        cancelled = false;
    }

    virtual void onStart(const IJob& job) override {
        executeOnMainThread([progress = wxWeakRef<ProgressPanel>(progress.get()),
                                instanceName = job.instanceName(),
                                className = job.className()] {
            // job might not exist at this point
            if (progress) {
                progress->onRunStart(className, instanceName);
            }
        });
    }

    virtual void onEnd(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        executeOnMainThread([progress = wxWeakRef<ProgressPanel>(progress.get())] {
            if (progress) {
                progress->onRunEnd();
            }
        });
    }

    virtual void onSetUp(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onTimeStep(const Storage& storage, Statistics& stats) override {
        if (!storage.getUserData()) {
            return;
        }

        SharedPtr<IStorageUserData> data = storage.getUserData();
        RawPtr<AnimationFrame> frame = dynamicCast<AnimationFrame>(data.get());
        SPH_ASSERT(frame);

        pane->update(std::move(frame->bitmap), std::move(frame->labels));
        executeOnMainThread([ref = wxWeakRef<ProgressPanel>(progress.get()), stats] {
            if (ref) {
                ref->update(stats);
            }
        });
    }

    virtual bool shouldAbortRun() const override {
        return cancelled;
    }

    void stop() {
        cancelled = true;
    }
};

RenderPage::RenderPage(wxWindow* parent, const RunSettings& global, const SharedPtr<INode>& node)
    : ClosablePage(parent, "render") {
    manager = makeAuto<wxAuiManager>(this);

    ImagePane* pane = new ImagePane(this);
    ProgressPanel* progress = new ProgressPanel(this);

    this->Bind(wxEVT_MOUSEWHEEL, [pane](wxMouseEvent& evt) {
        const float spin = evt.GetWheelRotation();
        pane->zoom(spin);
    });

    wxAuiPaneInfo info;
    info.Center().MinSize(wxSize(640, 480)).CaptionVisible(false).CloseButton(false).Show(true);
    manager->AddPane(pane, info);
    info.Bottom().MinSize(wxSize(-1, 40)).CaptionVisible(false).DockFixed(true).CloseButton(false).Show(true);
    manager->AddPane(progress, info);

    manager->Update();

    running = true;
    callbacks = makeAuto<RenderPageCallbacks>(pane, progress);

    renderThread = std::thread([this, node, global] {
        try {
            node->run(global, *callbacks);
        } catch (const Exception& e) {
            executeOnMainThread([message = std::string(e.what())] {
                wxMessageBox("Rendering failed.\n" + message, "Fail", wxOK | wxCENTRE);
            });
        }

        running = false;
        this->onStopped();
    });
}

RenderPage::~RenderPage() {
    renderThread.join();

    manager->UnInit();
    manager = nullptr;
}

bool RenderPage::isRunning() const {
    return running;
}

void RenderPage::stop() {
    callbacks->stop();
}

void RenderPage::quit() {}

NAMESPACE_SPH_END
