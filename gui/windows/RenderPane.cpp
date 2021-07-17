#include "gui/windows/RenderPane.h"
#include "gui/MainLoop.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderJobs.h"
#include "gui/windows/RenderPane.h"
#include "objects/containers/FlatSet.h"
#include "quantities/Storage.h"
#include "system/Timer.h"
#include "thread/CheckFunction.h"
#include <iostream>
#include <wx/dcclient.h>
#include <wx/weakref.h>

NAMESPACE_SPH_BEGIN

/// Calls Refresh on given main in main thread if the panel still exists.
void safeRefresh(wxPanel* panel) {
    executeOnMainThread([ref = wxWeakRef<wxPanel>(panel)] {
        if (ref) {
            ref->Refresh();
        }
    });
}

void BitmapOutput::update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) {
    update(bitmap.clone(), std::move(labels), isFinal);
}

void BitmapOutput::update(Bitmap<Rgba>&& bitmap, Array<Label>&& UNUSED(labels), const bool UNUSED(isFinal)) {
    // std::cout << "Updating the bitmap" << std::endl;
    {
        std::unique_lock<std::mutex> lock(mutex);
        render = std::move(bitmap);
    }
    safeRefresh(panel);
}

wxBitmap BitmapOutput::getBitmap() {
    std::unique_lock<std::mutex> lock(mutex);
    if (render.empty()) {
        return {};
    }

    wxBitmap bitmap;
    toWxBitmap(render, bitmap);
    return bitmap;
}

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

Outcome InteractiveRenderer::Status::isValid() const {
    if (!otherReason.empty()) {
        return makeFailed(otherReason);
    } else if (notInitialized) {
        return makeFailed("Initializing");
    } else if (particlesMissing) {
        return makeFailed("Particles not connected");
    } else if (cameraMissing) {
        return makeFailed("Camera not connected");
    } else if (colorizerMissing) {
        return makeFailed("No quantity selected");
    } else {
        return SUCCESS;
    }
}


InteractiveRenderer::InteractiveRenderer(const SharedPtr<JobNode>& node, wxPanel* panel)
    : node(node)
    , quitting(false)
    , output(panel) {}

void InteractiveRenderer::start(const RunSettings& globals) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);

    job = dynamicCast<AnimationJob, IJob>(node->getJob());

    // install the accessors
    this->setRendererAccessor(globals);
    for (Size i = 0; i < node->getSlotCnt(); ++i) {
        const SlotData slot = node->getSlot(i);
        if (slot.type == GuiJobType::CAMERA) {
            if (slot.provider) {
                this->setCameraAccessor(globals, slot.provider);
            } else {
                status.cameraMissing = true;
            }
        } else if (slot.type == JobType::PARTICLES) {
            if (slot.provider) {
                this->setNodeAccessor(slot.provider);
            } else {
                status.particlesMissing = true;
            }
        }
    }

    // parse everything when the thread starts
    changed.node = node;

    thread = std::thread([this, globals] { this->renderLoop(globals); });
}

InteractiveRenderer::~InteractiveRenderer() {
    this->stop();
}

void InteractiveRenderer::resize(const Pixel newResolution) {
    resolution = newResolution;
    changed.resolution = true;
    this->update();
}

AutoPtr<ICamera> InteractiveRenderer::getNewCamera(const SharedPtr<JobNode>& cameraNode,
    const RunSettings& globals) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    UpdateCameraCallbacks callbacks;
    cameraNode->run(globals, callbacks);
    return callbacks.getCamera();
}

void InteractiveRenderer::setCameraAccessor(const RunSettings& globals,
    const SharedPtr<JobNode>& cameraNode) {
    SPH_ASSERT(cameraNode);
    /// \todo cameraNode is holding itself, it will never expire (?)

    auto accessor = [=](const JobNotificationType type, const Any& UNUSED(value)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
        if (type != JobNotificationType::ENTRY_CHANGED) {
            // don't care
            return;
        }
        changed.camera = this->getNewCamera(cameraNode, globals);
        this->update();
    };

    cameraNode->addAccessor(this->sharedFromThis(), accessor);
}

void InteractiveRenderer::setRendererAccessor(const RunSettings& globals) {
    SPH_ASSERT(node);

    auto accessor = [=](const JobNotificationType type, const Any& value) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
        if (!preview) {
            // we previously failed to parse the object, so we have to do it from scratch anyway
            changed.node = cloneHierarchy(*node);
        } else if (type == JobNotificationType::ENTRY_CHANGED) {
            const std::string key = anyCast<std::string>(value).value();

            /// \todo avoid hardcoded string
            if (key == "quantities" || key == "surface_gravity") {
                changed.colorizer = job->getColorizer(globals);
                status.colorizerMissing = !changed.colorizer;
            } else {

                /// \todo put this in AnimationJob, something like listOfColorizerEntries, etc.

                const GuiSettingsId id = GuiSettings::getEntryId(key).valueOr(GuiSettingsId(-1));
                // SPH_ASSERT(id);
                static FlatSet<GuiSettingsId> SOFT_PARAMS = {
                    GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR,
                    GuiSettingsId::SURFACE_LEVEL,
                    GuiSettingsId::SURFACE_AMBIENT,
                    GuiSettingsId::SURFACE_SUN_INTENSITY,
                    GuiSettingsId::SURFACE_EMISSION,
                    GuiSettingsId::VOLUME_EMISSION,
                    GuiSettingsId::VOLUME_ABSORPTION,
                };
                /// \todo also palette

                if (key == "transparent" || SOFT_PARAMS.contains(id)) {
                    changed.parameters = job->getRenderParams();
                } else {
                    changed.renderer = job->getRenderer(globals);
                }
            }
        } else if (type == JobNotificationType::PROVIDER_CONNECTED) {
            SharedPtr<JobNode> provider = anyCast<SharedPtr<JobNode>>(value).value();
            const ExtJobType jobType = provider->provides().value();
            if (jobType == JobType::PARTICLES) {
                this->setNodeAccessor(provider);
                changed.node = cloneHierarchy(*node);
            } else if (jobType == GuiJobType::CAMERA) {
                // assuming the camera does not have providers ...
                SPH_ASSERT(provider->getSlotCnt() == 0);
                this->setCameraAccessor(globals, provider);
                changed.camera = this->getNewCamera(provider, globals);
            } else {
                SPH_ASSERT(false, "Connected unexpected node ", provider->instanceName());
            }
        } else if (type == JobNotificationType::PROVIDER_DISCONNECTED) {
            // changed.node = cloneHierarchy(*node);
            SharedPtr<JobNode> provider = anyCast<SharedPtr<JobNode>>(value).value();
            const ExtJobType jobType = provider->provides().value();
            if (jobType == GuiJobType::CAMERA) {
                status.cameraMissing = true;
            } else if (jobType == JobType::PARTICLES) {
                status.particlesMissing = true;
            } else {
                SPH_ASSERT(false, "Disconnected unexpected node ", provider->instanceName());
            }
        }

        this->update();
    };

    node->addAccessor(this->sharedFromThis(), accessor);
}

void InteractiveRenderer::setNodeAccessor(const SharedPtr<JobNode>& particleNode) {
    SPH_ASSERT(particleNode);

    auto accessor = [=](const JobNotificationType type, const Any& value) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);

        if (type == JobNotificationType::DEPENDENT_CONNECTED) {
            // don't care about connection to other nodes
            return;
        } else if (type == JobNotificationType::PROVIDER_CONNECTED) {
            // install accessor to the provider and all of its providers
            SharedPtr<JobNode> provider = anyCast<SharedPtr<JobNode>>(value).value();
            provider->enumerate([this](const SharedPtr<JobNode>& node) { this->setNodeAccessor(node); });
        }

        changed.node = cloneHierarchy(*node);
        this->update();
    };

    particleNode->addAccessor(this->sharedFromThis(), accessor);
}

void InteractiveRenderer::renderLoop(const RunSettings& globals) {
    quitting = false;
    CHECK_FUNCTION(CheckFunction::NO_THROW);
    while (!quitting) {
        SharedPtr<JobNode> evaluated = changed.node;
        if (evaluated) {
            // everything changed, re-evaluate
            try {
                std::cout << "Updating ALL" << std::endl;
                NullJobCallbacks callbacks;
                evaluated->prepare(globals, callbacks);
                RawPtr<AnimationJob> newJob = dynamicCast<AnimationJob, IJob>(evaluated->getJob());
                preview = newJob->getRenderPreview(globals);
                status.clear();
            } catch (const InvalidSetup& e) {
                status.otherReason = e.what();
                preview = nullptr;
                safeRefresh(output.getPanel());
            }
            // not changed in the meantime
            if (changed.node == evaluated) {
                changed.node = nullptr;
            }

        } else if (preview) {
            if (changed.camera) {
                std::cout << "Updating camera" << std::endl;
                status.cameraMissing = false;
                preview->update(std::move(changed.camera));
            }
            if (changed.parameters) {
                std::cout << "Updating parameters" << std::endl;
                preview->update(std::move(changed.parameters.value()));
                changed.parameters = NOTHING;
            }
            if (changed.colorizer) {
                std::cout << "Updating colorizer" << std::endl;
                status.colorizerMissing = false;
                preview->update(std::move(changed.colorizer));
            }
            if (changed.renderer) {
                std::cout << "Updating renderer" << std::endl;
                preview->update(std::move(changed.renderer));
            }
            if (changed.resolution) {
                std::cout << "Updating resolution" << std::endl;
                // not actually needed to do anything, just reset the flag
                changed.resolution = false;
            }
        }

        /// \todo all the members in 'changed' should be protected by mutex
        if (preview && !quitting && !changed.pending() && status.isValid()) {
            std::cout << "Re-render" << std::endl;
            preview->render(resolution, output);
        }

        if (!quitting && !changed.pending()) {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock);
        }
    }
}

void InteractiveRenderer::update() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    if (preview) {
        preview->cancel();
    }
    std::unique_lock<std::mutex> lock(mutex);
    cv.notify_one();
}

void InteractiveRenderer::stop() {
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


RenderPane::RenderPane(wxWindow* parent,
    const wxSize size,
    const SharedPtr<JobNode>& node,
    const RunSettings& globals)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size) {
    renderer = makeShared<InteractiveRenderer>(node, this);
    renderer->start(globals);

    this->Connect(wxEVT_PAINT, wxPaintEventHandler(RenderPane::onPaint));
    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& UNUSED(evt)) {
        const wxSize size = this->GetClientSize();
        renderer->resize(Pixel(size.x, size.y));
    });
}

void RenderPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // std::cout << "Paint event" << std::endl;`
    wxPaintDC dc(this);
    const wxSize size = dc.GetSize();
    wxBitmap bitmap = renderer->getBitmap();
    Outcome valid = renderer->isValid();

    if (bitmap.IsOk()) {
        if (!valid) {
            bitmap = bitmap.ConvertToDisabled();
        }
        const wxSize offset = size - bitmap.GetSize();
        dc.DrawBitmap(bitmap, wxPoint(offset.x / 2, offset.y / 2));
    }

    if (!valid) {
        const wxString text = valid.error() + " ...";
        const wxSize textSize = dc.GetTextExtent(text);
        dc.DrawText(text, size.x / 2 - textSize.x / 2, size.y / 2 - textSize.y / 2);
    }
}


NAMESPACE_SPH_END
