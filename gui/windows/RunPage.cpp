#include "gui/windows/RunPage.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayTracer.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/ParticleProbe.h"
#include "gui/windows/PlotView.h"
#include "gui/windows/ProgressPanel.h"
#include "gui/windows/TimeLine.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "sph/Diagnostics.h"
#include "thread/CheckFunction.h"
#include "thread/Pool.h"
#include "timestepping/TimeStepCriterion.h"
#include <fstream>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>
// needs to be included after framemanager
#include <wx/aui/dockart.h>
NAMESPACE_SPH_BEGIN

class SelectedParticleIntegral : public IIntegral<Float> {
private:
    SharedPtr<IColorizer> colorizer;
    Size selectedIdx;

public:
    SelectedParticleIntegral(SharedPtr<IColorizer> colorizer, const Size idx)
        : colorizer(colorizer)
        , selectedIdx(idx) {}

    virtual Float evaluate(const Storage& UNUSED(storage)) const override {
        ASSERT(colorizer->isInitialized(), colorizer->name());
        return colorizer->evalScalar(selectedIdx).valueOr(0._f);
    }

    virtual std::string getName() const override {
        return colorizer->name() + " " + std::to_string(selectedIdx);
    }
};

/// \brief Temporal plot of currently selected particle.
///
/// Uses current colorizer as a source quantity. If either colorizer or selected particle changes, plot is
/// cleared. It also holds a cache of previously selected particles, so that if a previously plotted particle
/// is re-selected, the cached data are redrawn.
class SelectedParticlePlot : public IPlot {
private:
    // Currently used plot (actual implementation); may be nullptr
    SharedPtr<TemporalPlot> currentPlot;

    // Selected particle; if NOTHING, no plot is drawn
    Optional<Size> selectedIdx;

    // Colorizer used to get the scalar value of the selected particle
    SharedPtr<IColorizer> colorizer;

    // Cache of previously selected particles. Cleared every time a new colorizer is selected.
    FlatMap<Size, SharedPtr<TemporalPlot>> plotCache;

public:
    void selectParticle(const Optional<Size> idx) {
        if (selectedIdx.valueOr(-1) == idx.valueOr(-1)) {
            // either the same particle or deselecting when nothing was selected; do nothing
            return;
        }
        if (selectedIdx && currentPlot) {
            // save the current plot to cache
            plotCache.insert(selectedIdx.value(), currentPlot);
        }
        selectedIdx = idx;

        if (idx) {
            // try to get the plot from the cache
            Optional<SharedPtr<TemporalPlot>&> cachedPlot = plotCache.tryGet(idx.value());
            if (cachedPlot) {
                // reuse the cached plot
                currentPlot = cachedPlot.value();
                return;
            }
        }
        // either deselecting or no cached plot found; clear the plot
        this->clear();
    }

    void setColorizer(const SharedPtr<IColorizer>& newColorizer) {
        if (colorizer != newColorizer) {
            colorizer = newColorizer;
            this->clear();
            plotCache.clear();
        }
    }

    virtual std::string getCaption() const override {
        if (currentPlot) {
            return currentPlot->getCaption();
        } else {
            return "Selected particle";
        }
    }

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override {
        if (selectedIdx && currentPlot) {
            currentPlot->onTimeStep(storage, stats);
        } else {
            currentPlot.reset();
        }
        // also do onTimeStep for all cached plots
        for (auto& plot : plotCache) {
            plot.value->onTimeStep(storage, stats);
        }
        this->syncRanges();
    }

    virtual void clear() override {
        if (selectedIdx) {
            AutoPtr<SelectedParticleIntegral> integral =
                makeAuto<SelectedParticleIntegral>(colorizer, selectedIdx.value());
            TemporalPlot::Params params;
            params.minRangeY = EPS;
            params.shrinkY = false;
            params.period = 0.01_f;
            params.maxPointCnt = 1000;
            currentPlot = makeAuto<TemporalPlot>(std::move(integral), params);
        } else {
            currentPlot.reset();
        }
        this->syncRanges();
    }

    virtual void plot(IDrawingContext& dc) const override {
        if (currentPlot) {
            currentPlot->plot(dc);
        }
    }

private:
    /// Since range getters are not virtual, we have to update ranges manually.
    void syncRanges() {
        if (currentPlot) {
            ranges.x = currentPlot->rangeX();
            ranges.y = currentPlot->rangeY();
        }
    }
};

class TimeLineCallbacks : public ITimeLineCallbacks {
private:
    Controller* parent;

public:
    explicit TimeLineCallbacks(Controller* parent)
        : parent(parent) {}

    virtual void frameChanged(const Path& newFile) const override {
        parent->open(newFile, false);
    }

    virtual void startSequence(const Path& firstFile) const override {
        parent->open(firstFile, true);
    }

    virtual void stop() const override {
        parent->stop(false);
    }

    virtual void pause() const override {
        parent->pause();
    }
};

RunPage::RunPage(wxWindow* window, Controller* parent, GuiSettings& settings)
    : controller(parent)
    , gui(settings) {

    wxSize size(
        settings.get<int>(GuiSettingsId::WINDOW_WIDTH), settings.get<int>(GuiSettingsId::WINDOW_HEIGHT));
    this->Create(window, wxID_ANY, wxDefaultPosition, size);

    manager = makeAuto<wxAuiManager>(this);

    wxPanel* visBar = createVisBar();
    pane = alignedNew<OrthoPane>(this, parent, settings);
    wxPanel* plotBar = createPlotBar();
    statsBar = createStatsBar();

    timelineBar = alignedNew<TimeLine>(this, Path(), makeShared<TimeLineCallbacks>(parent));
    progressBar = alignedNew<ProgressPanel>(this);

    wxAuiPaneInfo info;

    // info.Top().MinSize(wxSize(-1, 35)).CaptionVisible(false).DockFixed(true).CloseButton(false);
    // manager->AddPane(toolBar, info);

    info.Center().MinSize(wxSize(300, 300)).CaptionVisible(false).DockFixed(true).CloseButton(false);
    manager->AddPane(&*pane, info);

    info.Bottom().MinSize(wxSize(-1, 40)).CaptionVisible(false).DockFixed(true).CloseButton(false);
    manager->AddPane(timelineBar, info.Show(false));
    manager->AddPane(progressBar, info.Show(true));

    info.Left()
        .MinSize(wxSize(300, -1))
        .CaptionVisible(true)
        .DockFixed(false)
        .CloseButton(true)
        .Caption("Visualization");
    manager->AddPane(visBar, info);

    info.Right()
        .MinSize(wxSize(300, -1))
        .CaptionVisible(true)
        .DockFixed(false)
        .CloseButton(true)
        .Caption("Run statistics");
    manager->AddPane(statsBar, info);
    info.Caption("Particle data");
    manager->AddPane(plotBar, info);


    manager->Update();

    // connect event handlers
    wxAuiNotebook* notebook = findNotebook();
    ASSERT(notebook);
}

RunPage::~RunPage() {
    manager->UnInit();
    manager = nullptr;
}


const wxSize buttonSize(250, -1);
const wxSize spinnerSize(100, -1);
const int boxPadding = 10;

wxWindow* RunPage::createParticleBox(wxPanel* parent) {
    wxStaticBox* particleBox = new wxStaticBox(parent, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 160));

    wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* cutoffSizer = new wxBoxSizer(wxHORIZONTAL);
    cutoffSizer->AddSpacer(boxPadding);
    wxStaticText* text = new wxStaticText(particleBox, wxID_ANY, "Cutoff [km]");
    cutoffSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float cutoff = gui.get<Float>(GuiSettingsId::ORTHO_CUTOFF) * 1.e-3_f;

    FloatTextCtrl* cutoffCtrl = new FloatTextCtrl(particleBox, cutoff, Interval(0, LARGE));
    cutoffCtrl->onValueChanged = [this](const Float value) { this->updateCutoff(value * 1.e3_f); };
    cutoffCtrl->SetToolTip(
        "Specifies the cutoff distance in kilometers for rendering particles. When set to a positive number, "
        "only particles in a layer of specified thickness are rendered. Zero means all particles are "
        "rendered.");
    cutoffSizer->Add(cutoffCtrl, 1, wxALIGN_CENTER_VERTICAL);
    cutoffSizer->AddSpacer(boxPadding);
    boxSizer->Add(cutoffSizer);

    wxBoxSizer* particleSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    particleSizeSizer->AddSpacer(boxPadding);
    text = new wxStaticText(particleBox, wxID_ANY, "Particle radius");
    particleSizeSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float radius = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    FloatTextCtrl* particleSizeCtrl = new FloatTextCtrl(particleBox, radius, Interval(1.e-3_f, 1.e3_f));
    particleSizeCtrl->SetToolTip(
        "Multiplier of a particle radius. Must be set to 1 to get the actual size of particles in N-body "
        "simulations.");
    particleSizeCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::PARTICLE_RADIUS, value);
        controller->tryRedraw();
    };
    particleSizeSizer->Add(particleSizeCtrl, 1, wxALIGN_CENTER_VERTICAL);
    particleSizeSizer->AddSpacer(boxPadding);
    boxSizer->Add(particleSizeSizer);


    wxBoxSizer* grayscaleSizer = new wxBoxSizer(wxHORIZONTAL);
    grayscaleSizer->AddSpacer(boxPadding);
    wxCheckBox* grayscaleBox = new wxCheckBox(particleBox, wxID_ANY, "Force grayscale");
    grayscaleBox->SetToolTip(
        "Draws the particles in grayscale, useful to see how the image would look if printed using "
        "black-and-white printer.");
    grayscaleBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::FORCE_GRAYSCALE, value);
        controller->tryRedraw();
    });
    grayscaleSizer->Add(grayscaleBox);
    boxSizer->Add(grayscaleSizer);

    wxBoxSizer* keySizer = new wxBoxSizer(wxHORIZONTAL);
    keySizer->AddSpacer(boxPadding);
    wxCheckBox* keyBox = new wxCheckBox(particleBox, wxID_ANY, "Show key");
    keyBox->SetToolTip(
        "If checked, the color palette and the length scale are included in the rendered image.");
    keyBox->SetValue(true);
    keySizer->Add(keyBox);
    boxSizer->Add(keySizer);

    wxBoxSizer* aaSizer = new wxBoxSizer(wxHORIZONTAL);
    aaSizer->AddSpacer(boxPadding);
    wxCheckBox* aaBox = new wxCheckBox(particleBox, wxID_ANY, "Anti-aliasing");
    aaBox->SetToolTip(
        "If checked, particles are drawn with anti-aliasing, creating smoother image, but it also takes "
        "longer to render it.");
    aaSizer->Add(aaBox);

    aaSizer->AddSpacer(boxPadding);
    wxCheckBox* smoothBox = new wxCheckBox(particleBox, wxID_ANY, "Smooth particles");
    smoothBox->SetToolTip(
        "If checked, particles are drawn semi-transparently. The transparency of a particle follows "
        "smoothing kernel, imitating actual smoothing of SPH particles.");
    smoothBox->Enable(false);
    aaSizer->Add(smoothBox);
    boxSizer->Add(aaSizer);

    particleBox->SetSizer(boxSizer);

    keyBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SHOW_KEY, value);
        controller->tryRedraw();
    });
    aaBox->Bind(wxEVT_CHECKBOX, [this, smoothBox](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::ANTIALIASED, value);
        smoothBox->Enable(value);
        controller->tryRedraw();
    });
    smoothBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SMOOTH_PARTICLES, value);
        controller->tryRedraw();
    });

    return particleBox;
}

wxWindow* RunPage::createRaytracerBox(wxPanel* parent) {
    wxStaticBox* raytraceBox = new wxStaticBox(parent, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 120));
    wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* levelSizer = new wxBoxSizer(wxHORIZONTAL);
    levelSizer->AddSpacer(boxPadding);
    wxStaticText* text = new wxStaticText(raytraceBox, wxID_ANY, "Surface level");
    levelSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float level = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    FloatTextCtrl* levelCtrl = new FloatTextCtrl(raytraceBox, level, Interval(0._f, 10._f));
    levelCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_LEVEL, value);
        controller->tryRedraw();
    };
    levelSizer->Add(levelCtrl, 1, wxALIGN_CENTER_VERTICAL);
    levelSizer->AddSpacer(boxPadding);
    boxSizer->Add(levelSizer);

    wxBoxSizer* sunlightSizer = new wxBoxSizer(wxHORIZONTAL);
    sunlightSizer->AddSpacer(boxPadding);
    text = new wxStaticText(raytraceBox, wxID_ANY, "Sunlight");
    sunlightSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float sunlight = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
    FloatTextCtrl* sunlightCtrl = new FloatTextCtrl(raytraceBox, sunlight, Interval(0._f, 100._f));
    sunlightCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_SUN_INTENSITY, value);
        controller->tryRedraw();
    };
    sunlightSizer->Add(sunlightCtrl, 1, wxALIGN_CENTER_VERTICAL);
    sunlightSizer->AddSpacer(boxPadding);
    boxSizer->Add(sunlightSizer);

    wxBoxSizer* ambientSizer = new wxBoxSizer(wxHORIZONTAL);
    ambientSizer->AddSpacer(boxPadding);
    text = new wxStaticText(raytraceBox, wxID_ANY, "Ambient");
    ambientSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float ambient = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
    FloatTextCtrl* ambientCtrl = new FloatTextCtrl(raytraceBox, ambient, Interval(0._f, 100._f));
    ambientCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_AMBIENT, value);
        controller->tryRedraw();
    };
    ambientSizer->Add(ambientCtrl, 1, wxALIGN_CENTER_VERTICAL);
    ambientSizer->AddSpacer(boxPadding);
    boxSizer->Add(ambientSizer);

    raytraceBox->SetSizer(boxSizer);
    return raytraceBox;
}

static void enableRecursive(wxWindow* window, const bool enable) {
    window->Enable(enable);
    for (wxWindow* child : window->GetChildren()) {
        enableRecursive(child, enable);
    }
}

wxPanel* RunPage::createVisBar() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxPanel* visbarPanel = new wxPanel(this);
    visbarPanel->SetLabel("Visualization");

    wxBoxSizer* visbarSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* resetView = new wxButton(visbarPanel, wxID_ANY, "Reset view");
    resetView->SetToolTip("Resets the camera rotation.");
    resetView->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        pane->resetView();
        AutoPtr<ICamera> camera = controller->getCurrentCamera();
        camera->transform(AffineMatrix::identity());
        controller->refresh(std::move(camera));
    });
    buttonSizer->Add(resetView);

    wxButton* refresh = new wxButton(visbarPanel, wxID_ANY, "Refresh");
    refresh->SetToolTip("Updates the particle order and repaints the current view");
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { controller->tryRedraw(); });
    buttonSizer->Add(refresh);

    wxButton* snap = new wxButton(visbarPanel, wxID_ANY, "Save image");
    snap->SetToolTip("Saves the currently rendered image.");
    buttonSizer->Add(snap);
    snap->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doSaveFileDialog("Save image", { { "PNG image", "png" } });
        if (!path) {
            return;
        }
        const wxBitmap& bitmap = controller->getRenderedBitmap();
        saveToFile(bitmap, path.value());
    });

    visbarSizer->Add(buttonSizer);
    visbarSizer->AddSpacer(10);

    wxCheckBox* autoRefresh = new wxCheckBox(visbarPanel, wxID_ANY, "Refresh on timestep");
    autoRefresh->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::REFRESH_ON_TIMESTEP, evt.IsChecked());
    });
    autoRefresh->SetValue(gui.get<bool>(GuiSettingsId::REFRESH_ON_TIMESTEP));
    visbarSizer->Add(autoRefresh);
    visbarSizer->AddSpacer(10);


    wxBoxSizer* colorSizer = new wxBoxSizer(wxHORIZONTAL);
    colorSizer->Add(new wxStaticText(visbarPanel, wxID_ANY, "Background: "), 10, wxALIGN_CENTER_VERTICAL);
    wxColourPickerCtrl* picker = new wxColourPickerCtrl(visbarPanel, wxID_ANY);
    picker->SetColour(wxColour(controller->getParams().get<Rgba>(GuiSettingsId::BACKGROUND_COLOR)));
    picker->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent& evt) {
        wxColour color = evt.GetColour();
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(color));
        controller->tryRedraw();
    });
    colorSizer->Add(picker, 1, wxALIGN_CENTER_VERTICAL, 5);
    visbarSizer->Add(colorSizer);
    visbarSizer->AddSpacer(10);

    wxBoxSizer* quantitySizer = new wxBoxSizer(wxHORIZONTAL);

    quantitySizer->Add(new wxStaticText(visbarPanel, wxID_ANY, "Quantity: "), 10, wxALIGN_CENTER_VERTICAL);
    quantityBox = new wxComboBox(visbarPanel, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    quantityBox->SetToolTip(
        "Selects which quantity to visualize using associated color scale. Quantity values can be also "
        "obtained by left-clicking on a particle.");
    quantityBox->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
    quantityBox->SetSelection(0);
    quantityBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const int idx = quantityBox->GetSelection();
        this->setColorizer(idx);
    });

    quantitySizer->Add(quantityBox, 1, wxALIGN_CENTER_VERTICAL, 5);

    visbarSizer->Add(quantitySizer);
    visbarSizer->AddSpacer(10);

    wxRadioButton* particleButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Particles", wxDefaultPosition, buttonSize, wxRB_GROUP);
    particleButton->SetToolTip("Render individual particles with optional smoothing.");
    visbarSizer->Add(particleButton);
    wxWindow* particleBox = this->createParticleBox(visbarPanel);
    visbarSizer->Add(particleBox, 0, wxALL, 5);
    visbarSizer->AddSpacer(10);


    wxRadioButton* surfaceButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Raytraced surface", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(surfaceButton);
    wxWindow* raytracerBox = this->createRaytracerBox(visbarPanel);
    visbarSizer->Add(raytracerBox, 0, wxALL, 5);
    visbarSizer->AddSpacer(10);

    wxRadioButton* meshButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Surface mesh", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(meshButton);
    visbarSizer->AddSpacer(6);


    auto enableControls = [=](int renderIdx) {
        enableRecursive(particleBox, renderIdx == 0);
        enableRecursive(raytracerBox, renderIdx == 1);
    };
    enableControls(0);

    particleButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        IScheduler& scheduler = *ThreadPool::getGlobalInstance();
        controller->setRenderer(makeAuto<ParticleRenderer>(scheduler, gui));
        enableControls(0);
    });
    meshButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        IScheduler& scheduler = *ThreadPool::getGlobalInstance();
        controller->setRenderer(makeAuto<MeshRenderer>(scheduler, gui));
        enableControls(1);
    });
    surfaceButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        try {
            IScheduler& scheduler = *ThreadPool::getGlobalInstance();
            controller->setRenderer(makeAuto<RayTracer>(scheduler, gui));
            enableControls(2);
        } catch (std::exception& e) {
            wxMessageBox(std::string("Cannot initialize raytracer.\n\n") + e.what(), "Error", wxOK);

            // switch to particle renderer (fallback option)
            particleButton->SetValue(true);
            IScheduler& scheduler = *ThreadPool::getGlobalInstance();
            controller->setRenderer(makeAuto<ParticleRenderer>(scheduler, gui));
            enableControls(0);
        }
    });

    visbarSizer->AddSpacer(16);
    quantityPanel = new wxPanel(visbarPanel, wxID_ANY);
    visbarSizer->Add(quantityPanel);
    quantityPanelSizer = visbarSizer;

    visbarPanel->SetSizer(visbarSizer);
    return visbarPanel;
}

void RunPage::updateCutoff(const double cutoff) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // Note that we have to get camera from pane, not controller, as pane camera is always the one being
    // modified and fed to controller. Using controller's camera would cause cutoff to be later overriden by
    // the camera from pane.
    ICamera& camera = pane->getCamera();
    camera.setCutoff(cutoff > 0 ? Optional<float>(cutoff) : NOTHING);
    controller->refresh(camera.clone());
    controller->tryRedraw();
}

/// \brief Helper object used for drawing multiple plots into the same device.
class MultiPlot : public IPlot {
private:
    Array<AutoPtr<IPlot>> plots;

public:
    explicit MultiPlot(Array<AutoPtr<IPlot>>&& plots)
        : plots(std::move(plots)) {}

    virtual std::string getCaption() const override {
        return plots[0]->getCaption(); /// ??
    }

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override {
        ranges.x = ranges.y = Interval();
        for (auto& plot : plots) {
            plot->onTimeStep(storage, stats);
            ranges.x.extend(plot->rangeX());
            ranges.y.extend(plot->rangeY());
        }
    }

    virtual void clear() override {
        ranges.x = ranges.y = Interval();
        for (auto& plot : plots) {
            plot->clear();
        }
    }

    virtual void plot(IDrawingContext& dc) const override {
        for (auto plotAndIndex : iterateWithIndex(plots)) {
            dc.setStyle(plotAndIndex.index());
            plotAndIndex.value()->plot(dc);
        }
    }
};

static Array<Post::HistPoint> getOverplotSfd(const GuiSettings& gui) {
    const Path overplotPath(gui.get<std::string>(GuiSettingsId::PLOT_OVERPLOT_SFD));
    if (overplotPath.empty() || !FileSystem::pathExists(overplotPath)) {
        return {};
    }
    Array<Post::HistPoint> overplotSfd;
    std::ifstream is(overplotPath.native());
    while (true) {
        float value, count;
        is >> value >> count;
        if (!is) {
            break;
        }
        value *= 0.5 /*D->R*/ * 1000 /*km->m*/; // super-specific, should be generalized somehow
        overplotSfd.emplaceBack(Post::HistPoint{ value, Size(round(count)) });
    };
    return overplotSfd;
}

wxPanel* RunPage::createPlotBar() {
    wxPanel* sidebarPanel = new wxPanel(this);
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    probe = new ParticleProbe(sidebarPanel, wxSize(300, 155));
    sidebarSizer->Add(probe.get(), 0, wxALIGN_TOP);
    sidebarSizer->AddSpacer(5);

    // the list of all available integrals to plot
    // shared between plotviews, so that they can switch plot through context menu
    SharedPtr<Array<PlotData>> list = makeShared<Array<PlotData>>();

    TemporalPlot::Params params;
    params.minRangeY = 1.4_f;
    params.shrinkY = false;
    params.period = gui.get<Float>(GuiSettingsId::PLOT_INITIAL_PERIOD);

    PlotData data;
    IntegralWrapper integral;
    Flags<PlotEnum> flags = gui.getFlags<PlotEnum>(GuiSettingsId::PLOT_INTEGRALS);

    Array<Post::HistPoint> overplotSfd = getOverplotSfd(gui);

    /// \todo this plots should really be set somewhere outside of main window, they are problem-specific
    if (flags.has(PlotEnum::TOTAL_ENERGY)) {
        integral = makeAuto<TotalEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Rgba(wxColour(240, 255, 80));
        list->push(data);
    }

    if (flags.has(PlotEnum::KINETIC_ENERGY)) {
        integral = makeAuto<TotalKineticEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Rgba(wxColour(200, 0, 0));
        list->push(data);
    }

    if (flags.has(PlotEnum::INTERNAL_ENERGY)) {
        integral = makeAuto<TotalInternalEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Rgba(wxColour(255, 50, 50));
        list->push(data);
    }

    if (flags.has(PlotEnum::TOTAL_MOMENTUM)) {
        integral = makeAuto<TotalMomentum>();
        params.minRangeY = 1.e6_f;
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Rgba(wxColour(100, 200, 0));
        list->push(data);
    }

    if (flags.has(PlotEnum::TOTAL_ANGULAR_MOMENTUM)) {
        integral = makeAuto<TotalAngularMomentum>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Rgba(wxColour(130, 80, 255));
        list->push(data);
    }

    /*  if (flags.has(PlotEnum::PARTICLE_SFD)) {
          data.plot = makeLocking<SfdPlot>(0._f);
          plots.push(data.plot);
          data.color = Rgba(wxColour(0, 190, 255));
          list->push(data);
      }

      if (flags.has(PlotEnum::CURRENT_SFD)) {
          Array<AutoPtr<IPlot>> multiplot;
          multiplot.emplaceBack(makeAuto<SfdPlot>(Post::ComponentFlag::OVERLAP, params.period));
          if (!overplotSfd.empty()) {
              multiplot.emplaceBack(
                  makeAuto<DataPlot>(overplotSfd, AxisScaleEnum::LOG_X | AxisScaleEnum::LOG_Y, "Overplot"));
          }
          data.plot = makeLocking<MultiPlot>(std::move(multiplot));
          plots.push(data.plot);
          data.color = Rgba(wxColour(255, 40, 255));
          list->push(data);
      }

      if (flags.has(PlotEnum::PREDICTED_SFD)) {
          /// \todo could be deduplicated a bit
          Array<AutoPtr<IPlot>> multiplot;
          multiplot.emplaceBack(makeAuto<SfdPlot>(Post::ComponentFlag::ESCAPE_VELOCITY, params.period));
          if (!overplotSfd.empty()) {
              multiplot.emplaceBack(
                  makeAuto<DataPlot>(overplotSfd, AxisScaleEnum::LOG_X | AxisScaleEnum::LOG_Y, "Overplot"));
          }
          data.plot = makeLocking<MultiPlot>(std::move(multiplot));
          plots.push(data.plot);
          data.color = Rgba(wxColour(80, 150, 255));
          list->push(data);
      }

      if (flags.has(PlotEnum::PERIOD_HISTOGRAM)) {
          data.plot = makeLocking<HistogramPlot>(
              Post::HistogramId::ROTATIONAL_PERIOD, Interval(0._f, 10._f), "Rotational periods");
          plots.push(data.plot);
          data.color = Rgba(wxColour(255, 255, 0));
          list->push(data);
      }

      if (flags.has(PlotEnum::LARGEST_REMNANT_ROTATION)) {
          class LargestRemnantRotation : public IIntegral<Float> {
          public:
              virtual Float evaluate(const Storage& storage) const override {
                  ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
                  if (!storage.has(QuantityId::ANGULAR_FREQUENCY)) {
                      return 0._f;
                  }
                  ArrayView<const Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);

                  const Size largestRemnantIdx = std::distance(m.begin(), std::max_element(m.begin(),
      m.end())); const Float w = getLength(omega[largestRemnantIdx]); if (w == 0._f) { return 0._f; } else {
                      return 2._f * PI / (3600._f * w);
                  }
              }

              virtual std::string getName() const override {
                  return "Largest remnant period";
              }
          };
          integral = makeAuto<LargestRemnantRotation>();
          auto clonedParams = params;
          clonedParams.minRangeY = 1.e-2_f;
          data.plot = makeLocking<TemporalPlot>(integral, clonedParams);
          plots.push(data.plot);
          data.color = Rgba(wxColour(255, 0, 255));
          list->push(data);
      }*/

    if (flags.has(PlotEnum::SELECTED_PARTICLE)) {
        selectedParticlePlot = makeLocking<SelectedParticlePlot>();
        data.plot = selectedParticlePlot;
        plots.push(data.plot);
        data.color = Rgba(wxColour(255, 255, 255));
        list->push(data);
    } else {
        selectedParticlePlot.reset();
    }


    TicsParams tics;
    tics.minCnt = 2;
    tics.digits = 1;
    firstPlot = new PlotView(sidebarPanel, wxSize(300, 200), wxSize(10, 10), list, 0, tics);
    sidebarSizer->Add(firstPlot, 0, wxALIGN_TOP);
    sidebarSizer->AddSpacer(5);

    secondPlot =
        new PlotView(sidebarPanel, wxSize(300, 200), wxSize(10, 10), list, list->size() == 1 ? 0 : 1, tics);
    sidebarSizer->Add(secondPlot, 0, wxALIGN_TOP);

    sidebarPanel->SetSizerAndFit(sidebarSizer);
    return sidebarPanel;
}

wxPanel* RunPage::createStatsBar() {
    wxPanel* statsPanel = new wxPanel(this);
    wxBoxSizer* statsSizer = new wxBoxSizer(wxVERTICAL);

    wxFont font = wxSystemSettings::GetFont(wxSYS_SYSTEM_FONT);
    font.Scale(0.95f);
    statsPanel->SetFont(font);

    statsText = new wxTextCtrl(
        statsPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    this->makeStatsText(0, Statistics{});

    statsSizer->Add(statsText, 1, wxEXPAND | wxALL, 5);
    statsPanel->SetSizer(statsSizer);

    return statsPanel;
}

template <typename TValue>
static void printStat(wxTextCtrl* text,
    const Statistics& stats,
    const std::string& desc,
    const StatisticsId id,
    const std::string units = "") {
    if (stats.has(id)) {
        *text << desc << stats.get<TValue>(id) << units << "\n";
    } else {
        *text << desc << "N/A\n";
    }
}

void RunPage::makeStatsText(const Size particleCnt, const Statistics& stats) {
    statsText->Clear();
    *statsText << " - particles: ";
    if (particleCnt > 0) {
        *statsText << int(particleCnt) << "\n";
    } else {
        *statsText << "N/A\n";
    }

    printStat<Float>(statsText, stats, " - run time:  ", StatisticsId::RUN_TIME, "s");
    printStat<Float>(statsText, stats, " - timestep:  ", StatisticsId::TIMESTEP_VALUE, "s");

    if (stats.has(StatisticsId::TIMESTEP_CRITERION)) {
        CriterionId id = stats.get<CriterionId>(StatisticsId::TIMESTEP_CRITERION);
        std::stringstream ss;
        if (id == CriterionId::DERIVATIVE) {
            ss << stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY);
        } else {
            ss << id;
        }
        *statsText << "    * set by: " << ss.str() << "\n";
    }

    printStat<int>(statsText, stats, " - time spent:  ", StatisticsId::TIMESTEP_ELAPSED, "ms");
    printStat<int>(statsText, stats, "    * SPH evaluation: ", StatisticsId::SPH_EVAL_TIME, "ms");
    printStat<int>(statsText, stats, "    * gravity evaluation: ", StatisticsId::GRAVITY_EVAL_TIME, "ms");
    printStat<int>(statsText, stats, "    * collision evaluation: ", StatisticsId::COLLISION_EVAL_TIME, "ms");
    printStat<int>(statsText, stats, "    * tree construction:    ", StatisticsId::GRAVITY_BUILD_TIME, "ms");
    printStat<int>(
        statsText, stats, "    * visualization:        ", StatisticsId::POSTPROCESS_EVAL_TIME, "ms");

    printStat<int>(statsText, stats, " - collisions:  ", StatisticsId::TOTAL_COLLISION_COUNT);
    printStat<int>(statsText, stats, "    * bounces:  ", StatisticsId::BOUNCE_COUNT);
    printStat<int>(statsText, stats, "    * mergers:  ", StatisticsId::MERGER_COUNT);
    printStat<int>(statsText, stats, "    * breakups: ", StatisticsId::BREAKUP_COUNT);
    printStat<int>(statsText, stats, " - overlaps:    ", StatisticsId::OVERLAP_COUNT);
    printStat<int>(statsText, stats, " - aggregates:  ", StatisticsId::AGGREGATE_COUNT);

    /*           CriterionId id = stats.get<CriterionId>(StatisticsId::TIMESTEP_CRITERION);
               std::stringstream ss;
               if (id == CriterionId::DERIVATIVE) {
    ss << stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY);
               } else {
    ss << id;
               }
               const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
               logger->write(" - timestep:    ", dt, " (set by ", ss.str(), ")");

               printStat<int>(*logger, stats, StatisticsId::TIMESTEP_ELAPSED,             " - time spent:  ",
    "ms"); printStat<int>(*logger, stats, StatisticsId::SPH_EVAL_TIME,                "    * SPH evaluation:
    ", "ms"); printStat<int>(*logger, stats, StatisticsId::GRAVITY_EVAL_TIME,            "    * gravity
    evaluation:
    ", "ms"); printStat<int>(*logger, stats, StatisticsId::COLLISION_EVAL_TIME,          "    * collision
    evaluation: ", "ms"); printStat<int>(*logger, stats, StatisticsId::GRAVITY_BUILD_TIME,           "    *
    tree construction:    ", "ms"); printStat<int>(*logger, stats, StatisticsId::POSTPROCESS_EVAL_TIME, "    *
    visualization:        ", "ms"); logger->write( " - particles:   ", storage.getParticleCnt());
    printStat<MinMaxMean>(*logger, stats, StatisticsId::NEIGHBOUR_COUNT,       " - neigbours:   ");
    printStat<int>(*logger, stats, StatisticsId::TOTAL_COLLISION_COUNT,        " - collisions:  ");
    printStat<int>(*logger, stats, StatisticsId::BOUNCE_COUNT,                 "    * bounces:  ");
    printStat<int>(*logger, stats, StatisticsId::MERGER_COUNT,                 "    * mergers:  ");
    printStat<int>(*logger, stats, StatisticsId::BREAKUP_COUNT,                "    * breakups: ");
    printStat<int>(*logger, stats, StatisticsId::OVERLAP_COUNT,                " - overlaps:    ");
    printStat<int>(*logger, stats, StatisticsId::AGGREGATE_COUNT,              " - aggregates:  ");
    printStat<int>(*logger, stats, StatisticsId::SOLVER_SUMMATION_ITERATIONS,  " - iteration #: ");*/
}

void RunPage::setColorizer(const Size idx) {
    // do this even if idx==selectedIdx, we might change the colorizerList (weird behavior, but it will do for
    // now)
    controller->setColorizer(colorizerList[idx]);
    if (idx == selectedIdx) {
        return;
    }
    this->replaceQuantityBar(idx);
    selectedIdx = idx;
}

void RunPage::addComponentIdBar(wxWindow* parent, wxSizer* sizer, SharedPtr<IColorizer> colorizer) {
    sizer->AddSpacer(5);
    RawPtr<ComponentIdColorizer> componentId = dynamicCast<ComponentIdColorizer>(colorizer.get());

    wxRadioButton* overlapButton = new wxRadioButton(
        parent, wxID_ANY, "Connected particles", wxDefaultPosition, wxSize(-1, 25), wxRB_GROUP);
    sizer->Add(overlapButton);

    wxRadioButton* boundButton =
        new wxRadioButton(parent, wxID_ANY, "Bound particles", wxDefaultPosition, wxSize(-1, 25), 0);
    sizer->Add(boundButton);

    sizer->AddSpacer(15);
    wxCheckBox* highlightBox = new wxCheckBox(parent, wxID_ANY, "Highlight component");
    highlightBox->SetValue(bool(componentId->getHighlightIdx()));
    sizer->Add(highlightBox);

    wxBoxSizer* highlightSizer = new wxBoxSizer(wxHORIZONTAL);
    highlightSizer->AddSpacer(30);
    wxStaticText* text = new wxStaticText(parent, wxID_ANY, "Index");
    highlightSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    wxSpinCtrl* highlightIndex = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, spinnerSize);
    highlightIndex->SetValue(componentId->getHighlightIdx().valueOr(0));
    highlightIndex->Enable(highlightBox->GetValue());
    highlightSizer->Add(highlightIndex);
    sizer->Add(highlightSizer);

    overlapButton->Bind(wxEVT_RADIOBUTTON, [this, componentId, colorizer](wxCommandEvent& UNUSED(evt)) {
        componentId->setConnectivity(Post::ComponentFlag::SORT_BY_MASS | Post::ComponentFlag::OVERLAP);
        controller->setColorizer(colorizer);
    });
    boundButton->Bind(wxEVT_RADIOBUTTON, [this, componentId, colorizer](wxCommandEvent& UNUSED(evt)) {
        componentId->setConnectivity(
            Post::ComponentFlag::SORT_BY_MASS | Post::ComponentFlag::ESCAPE_VELOCITY);
        controller->setColorizer(colorizer);
    });
    highlightBox->Bind(wxEVT_CHECKBOX, [this, colorizer, componentId, highlightIndex](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();

        ASSERT(componentId);
        if (value) {
            componentId->setHighlightIdx(highlightIndex->GetValue());
        } else {
            componentId->setHighlightIdx(NOTHING);
        }
        highlightIndex->Enable(value);
        /// \todo this causes rebuild of colorizer, which is very inefficient, there should be some concept of
        /// validity that would tell whether it is necessary or not to rebuild it
        controller->setColorizer(colorizer);
    });
    highlightIndex->Bind(wxEVT_SPINCTRL, [this, componentId, colorizer](wxSpinEvent& evt) {
        // this is already executed on main thread, but we query it anyway to avoid spinner getting stuck
        executeOnMainThread([this, componentId, colorizer, evt] {
            const int index = evt.GetValue();
            componentId->setHighlightIdx(index);
            /// \todo also unnecessary
            controller->setColorizer(colorizer);
        });
    });
}

void RunPage::replaceQuantityBar(const Size idx) {
    quantityPanel->Destroy();

    quantityPanel = new wxPanel(this, wxID_ANY);
    // so far only needed for component id, so it is hacked like this
    SharedPtr<IColorizer> newColorizer = colorizerList[idx];
    /// \todo implement SharedPtr dynamicCast
    if (dynamicCast<ComponentIdColorizer>(newColorizer.get())) {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new wxStaticText(quantityPanel, wxID_ANY, newColorizer->name()));
        wxBoxSizer* offsetSizer = new wxBoxSizer(wxHORIZONTAL);
        offsetSizer->AddSpacer(25);
        sizer->Add(offsetSizer);
        wxBoxSizer* actSizer = new wxBoxSizer(wxVERTICAL);
        addComponentIdBar(quantityPanel, actSizer, newColorizer);
        offsetSizer->Add(actSizer);
        quantityPanel->SetSizerAndFit(sizer);
    }

    quantityPanelSizer->Add(quantityPanel);
    this->Layout();
}


void RunPage::setProgress(const Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    progressBar->update(stats);

    /// \todo check if IsShown works if hidden by AUI
    if (timelineBar->IsShown() && stats.has(StatisticsId::INDEX)) {
        timelineBar->setFrame(stats.get<int>(StatisticsId::INDEX));
    }
}

void RunPage::newPhase(const std::string& className, const std::string& instanceName) {
    progressBar->onRunStart(className, instanceName);
}

void RunPage::refresh() {
    pane->Refresh();
}

void RunPage::showTimeLine(const bool show) {
    wxAuiPaneInfo& timelineInfo = manager->GetPane(timelineBar);
    wxAuiPaneInfo& progressInfo = manager->GetPane(progressBar);
    wxAuiPaneInfo& statsInfo = manager->GetPane(statsBar);

    if (!timelineInfo.IsShown()) {
        timelineInfo.Show(show);
        progressInfo.Show(!show);
        statsInfo.Show(!show);
        manager->Update();
    }
}

void RunPage::runStarted(const Storage& storage, const Path& path) {
    Statistics dummy;
    pane->onTimeStep(storage, dummy);

    if (!path.empty()) {
        timelineBar->update(path);
    }

    for (auto plot : plots) {
        plot->clear();
    }
}

void RunPage::onTimeStep(const Storage& storage, const Statistics& stats) {
    // this is called from run thread (NOT main thread)

    // limit the refresh rate to avoid blocking the main thread
    if (statsText && statsTimer.elapsed(TimerUnit::MILLISECOND) > 100) {
        const Size particleCnt = storage.getParticleCnt();
        executeOnMainThread([this, stats, particleCnt] { this->makeStatsText(particleCnt, stats); });
        statsTimer.restart();
    }

    pane->onTimeStep(storage, stats);

    if (selectedParticlePlot) {
        selectedParticlePlot->selectParticle(controller->getSelectedParticle());

        /// \todo we should only touch colorizer from main thread!
        SharedPtr<IColorizer> colorizer = controller->getCurrentColorizer();
        // we need validity of arrayrefs only for the duration of this function, so weak reference is OK
        colorizer->initialize(storage, RefEnum::WEAK);
        selectedParticlePlot->setColorizer(colorizer);
    }

    if (storage.has(QuantityId::MASS)) {
        // skip plots if we don't have mass, for simplicity; this can be generalized if needed
        for (auto plot : plots) {
            plot->onTimeStep(storage, stats);
        }

        executeOnMainThread([this] {
            ASSERT(firstPlot && secondPlot);
            firstPlot->Refresh();
            secondPlot->Refresh();
        });
    }
}

void RunPage::onRunEnd() {
    if (waitingDialog) {
        waitingDialog->EndModal(0);
    }
}

void RunPage::setColorizerList(Array<SharedPtr<IColorizer>>&& colorizers) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    colorizerList = std::move(colorizers);
    wxArrayString items;
    for (auto& e : colorizerList) {
        items.Add(e->name().c_str());
    }
    quantityBox->Set(items);
    const Size actSelectedIdx = (selectedIdx < colorizerList.size()) ? selectedIdx : 0;
    quantityBox->SetSelection(actSelectedIdx);
}

void RunPage::setSelectedParticle(const Particle& particle, const Rgba color) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->update(particle, color);
}

void RunPage::deselectParticle() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->clear();
}

wxSize RunPage::getCanvasSize() const {
    const wxSize size = pane->GetSize();
    return wxSize(max(size.x, 1), max(size.y, 1));
}

class WaitDialog : public wxDialog {
public:
    WaitDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, "Info", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU) {
        const wxSize size = wxSize(320, 90);
        this->SetSize(size);
        wxStaticText* text = new wxStaticText(this, wxID_ANY, "Waiting for simulation to finish ...");
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->AddStretchSpacer();
        sizer->Add(text, 1, wxALIGN_CENTER_HORIZONTAL);
        sizer->AddStretchSpacer();
        this->SetSizer(sizer);
        this->Layout();
        this->CentreOnScreen();
    }
};

bool RunPage::close() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    // veto the event, we will close the window ourselves
    if (controller->isRunning()) {
        const int retval =
            wxMessageBox("Simulation is currently in progress. Do you want to stop it and close the window?",
                "Stop?",
                wxYES_NO | wxCENTRE);
        if (retval == wxYES) {
            controller->stop();
            waitingDialog = new WaitDialog(this);
            waitingDialog->ShowModal();
            controller->quit(true);
            wxAuiNotebook* notebook = findNotebook();
            notebook->DeletePage(notebook->GetPageIndex(this));
        }
        return false;
    } else {
        return true;
    }
}

NAMESPACE_SPH_END
