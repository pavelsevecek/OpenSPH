#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayTracer.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/ParticleProbe.h"
#include "gui/windows/PlotView.h"
#include "io/FileSystem.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "sph/Diagnostics.h"
#include "thread/CheckFunction.h"
#include "thread/Pool.h"
#include <fstream>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/textctrl.h>

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

MainWindow::MainWindow(Controller* parent, const GuiSettings& settings, RawPtr<IPluginControls> plugin)
    : controller(parent)
    , gui(settings) {
    // create the frame
    std::string title = settings.get<std::string>(GuiSettingsId::WINDOW_TITLE);
    wxSize size(
        settings.get<int>(GuiSettingsId::WINDOW_WIDTH), settings.get<int>(GuiSettingsId::WINDOW_HEIGHT));
    this->Create(nullptr, wxID_ANY, title.c_str(), wxDefaultPosition, size);

    // toolbar
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* toolbarSizer = createToolBar();
    sizer->Add(toolbarSizer);

    // everything below toolbar
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* visBarSizer = createVisBar();
    mainSizer->Add(visBarSizer);
    mainSizer->AddSpacer(5);

    wxBoxSizer* middleSizer = new wxBoxSizer(wxVERTICAL);
    pane = alignedNew<OrthoPane>(this, parent, settings);
    middleSizer->Add(pane.get(), 4);

    /// \todo generalize window setup
    if (plugin) {
        plugin->create(this, middleSizer);
    }
    mainSizer->Add(middleSizer);
    mainSizer->AddSpacer(5);

    wxBoxSizer* sidebarSizer = createPlotBar();
    mainSizer->Add(sidebarSizer);
    sizer->Add(mainSizer);


    this->SetSizer(sizer);
    this->Layout();

    // connect event handlers
    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainWindow::onClose));
}

wxBoxSizer* MainWindow::createToolBar() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxBoxSizer* toolbar = new wxBoxSizer(wxHORIZONTAL);

    wxButton* button = new wxButton(this, wxID_ANY, "Start");
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { controller->restart(); });
    toolbar->Add(button);

    button = new wxButton(this, wxID_ANY, "Pause");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { controller->pause(); });

    button = new wxButton(this, wxID_ANY, "Stop");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { controller->stop(); });

    button = new wxButton(this, wxID_ANY, "Save");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        static Array<FileFormat> fileFormats = {
            { "SPH state file", "ssf" }, { "SPH compressed file", "scf" }, { "Text file", "txt" }
        };
        Optional<Path> path = doSaveFileDialog("Save state file", fileFormats.clone());
        if (!path) {
            return;
        }
        controller->saveState(path.value());
    });

    button = new wxButton(this, wxID_ANY, "Load");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        static Array<FileFormat> fileFormats = { { "SPH state file", "ssf" } };
        Optional<Path> path = doOpenFileDialog("Load state file", fileFormats.clone());
        if (!path) {
            return;
        }
        controller->loadState(path.value());
    });

    button = new wxButton(this, wxID_ANY, "Snap");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doSaveFileDialog("Save image", { { "PNG image", "png" } });
        if (!path) {
            return;
        }
        const wxBitmap& bitmap = controller->getRenderedBitmap();
        saveToFile(bitmap, path.value());
    });

    toolbar->AddSpacer(434);

    quantityBox = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
    quantityBox->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
    quantityBox->SetSelection(0);
    quantityBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const int idx = quantityBox->GetSelection();
        this->setColorizer(idx);
    });
    toolbar->Add(quantityBox);

    wxButton* resetView = new wxButton(this, wxID_ANY, "Reset view");
    resetView->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        pane->resetView();
        AutoPtr<ICamera> camera = controller->getCurrentCamera();
        camera->reset();
        controller->refresh(std::move(camera));
    });
    toolbar->Add(resetView);

    wxButton* refresh = new wxButton(this, wxID_ANY, "Refresh");
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { controller->tryRedraw(); });
    toolbar->Add(refresh);

    gauge = new wxGauge(this, wxID_ANY, 1000);
    gauge->SetValue(0);
    gauge->SetMinSize(wxSize(300, -1));
    toolbar->AddSpacer(10);
    toolbar->Add(gauge, 0, wxALIGN_CENTER_VERTICAL);
    return toolbar;
}

const wxSize buttonSize(250, 35);
const wxSize textSize(120, -1);
const wxSize spinnerSizer(145, -1);

wxBoxSizer* MainWindow::createVisBar() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxBoxSizer* visbarSizer = new wxBoxSizer(wxVERTICAL);

    wxRadioButton* particleButton =
        new wxRadioButton(this, wxID_ANY, "Particles", wxDefaultPosition, buttonSize, wxRB_GROUP);
    visbarSizer->Add(particleButton);

    wxBoxSizer* cutoffSizer = new wxBoxSizer(wxHORIZONTAL);
    cutoffSizer->AddSpacer(25);
    wxStaticText* text = new wxStaticText(this, wxID_ANY, "Cutoff", wxDefaultPosition, textSize);
    cutoffSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    const Float cutoff = gui.get<Float>(GuiSettingsId::CAMERA_CUTOFF);
    wxSpinCtrlDouble* cutoffSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, spinnerSizer);
    cutoffSpinner->SetRange(0., 1000000.);
    cutoffSpinner->SetValue(cutoff);
    cutoffSpinner->SetDigits(3);
    cutoffSpinner->SetIncrement(1);
    cutoffSpinner->Bind(
        wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent& evt) { this->updateCutoff(evt.GetValue()); });
    cutoffSpinner->Bind(wxEVT_MOTION, [this, cutoffSpinner](wxMouseEvent& evt) {
        static wxPoint prevPos = evt.GetPosition();
        if (evt.Dragging()) {
            wxPoint drag = evt.GetPosition() - prevPos;
            const int value = cutoffSpinner->GetValue();
            const int cutoff = max(int(value * (1.f + float(drag.y) / 500.f)), 0);
            cutoffSpinner->SetValue(cutoff);
            this->updateCutoff(cutoff);
        }
        prevPos = evt.GetPosition();
    });
    cutoffSizer->Add(cutoffSpinner);
    visbarSizer->Add(cutoffSizer);

    wxBoxSizer* particleSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    particleSizeSizer->AddSpacer(25);
    text = new wxStaticText(this, wxID_ANY, "Particle radius", wxDefaultPosition, textSize);
    particleSizeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    wxSpinCtrlDouble* particleSizeSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, spinnerSizer);
    const Float radius = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    particleSizeSpinner->SetValue(radius);
    particleSizeSpinner->SetDigits(3);
    particleSizeSpinner->SetRange(0.001, 1000.);
    particleSizeSpinner->SetIncrement(0.001);
    particleSizeSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::PARTICLE_RADIUS, evt.GetValue());
        controller->tryRedraw();
    });
    particleSizeSizer->Add(particleSizeSpinner);
    visbarSizer->Add(particleSizeSizer);


    wxBoxSizer* grayscaleSizer = new wxBoxSizer(wxHORIZONTAL);
    grayscaleSizer->AddSpacer(25);
    wxCheckBox* grayscaleBox = new wxCheckBox(this, wxID_ANY, "Force grayscale");
    grayscaleBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::FORCE_GRAYSCALE, value);
        controller->tryRedraw();
    });
    grayscaleSizer->Add(grayscaleBox);
    visbarSizer->Add(grayscaleSizer);

    wxBoxSizer* keySizer = new wxBoxSizer(wxHORIZONTAL);
    keySizer->AddSpacer(25);
    wxCheckBox* keyBox = new wxCheckBox(this, wxID_ANY, "Show key");
    keyBox->SetValue(true);
    keySizer->Add(keyBox);
    visbarSizer->Add(keySizer);

    wxBoxSizer* aaSizer = new wxBoxSizer(wxHORIZONTAL);
    aaSizer->AddSpacer(25);
    wxCheckBox* aaBox = new wxCheckBox(this, wxID_ANY, "Anti-aliasing");
    aaSizer->Add(aaBox);
    visbarSizer->Add(aaSizer);

    wxBoxSizer* smoothSizer = new wxBoxSizer(wxHORIZONTAL);
    smoothSizer->AddSpacer(25);
    wxCheckBox* smoothBox = new wxCheckBox(this, wxID_ANY, "Smooth particles");
    smoothBox->Enable(false);
    smoothSizer->Add(smoothBox);
    visbarSizer->Add(smoothSizer);

    keyBox->Bind(wxEVT_CHECKBOX, [this, smoothBox](wxCommandEvent& evt) {
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


    wxRadioButton* meshButton =
        new wxRadioButton(this, wxID_ANY, "Surface mesh", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(meshButton);

    wxRadioButton* surfaceButton =
        new wxRadioButton(this, wxID_ANY, "Raytraced surface", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(surfaceButton);

    wxBoxSizer* levelSizer = new wxBoxSizer(wxHORIZONTAL);
    levelSizer->AddSpacer(25);
    text = new wxStaticText(this, wxID_ANY, "Surface level", wxDefaultPosition, textSize);
    levelSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    const Float level = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    wxSpinCtrlDouble* levelSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, spinnerSizer);
    levelSpinner->SetRange(0., 10.);
    levelSpinner->SetValue(level);
    levelSpinner->SetDigits(3);
    levelSpinner->SetIncrement(0.001);
    levelSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_LEVEL, evt.GetValue());
        controller->tryRedraw();
    });
    levelSizer->Add(levelSpinner);
    visbarSizer->Add(levelSizer);

    wxBoxSizer* sunlightSizer = new wxBoxSizer(wxHORIZONTAL);
    sunlightSizer->AddSpacer(25);
    text = new wxStaticText(this, wxID_ANY, "Sunlight", wxDefaultPosition, textSize);
    sunlightSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    const Float sunlight = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
    wxSpinCtrlDouble* sunlightSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(145, -1));
    sunlightSpinner->SetRange(0., 10.);
    sunlightSpinner->SetValue(sunlight);
    sunlightSpinner->SetDigits(3);
    sunlightSpinner->SetIncrement(0.001);
    sunlightSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_SUN_INTENSITY, evt.GetValue());
        controller->tryRedraw();
    });
    sunlightSizer->Add(sunlightSpinner);
    visbarSizer->Add(sunlightSizer);

    wxBoxSizer* ambientSizer = new wxBoxSizer(wxHORIZONTAL);
    ambientSizer->AddSpacer(25);
    text = new wxStaticText(this, wxID_ANY, "Ambient", wxDefaultPosition, textSize);
    ambientSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    const Float ambient = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
    wxSpinCtrlDouble* ambientSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(145, -1));
    ambientSpinner->SetRange(0., 10.);
    ambientSpinner->SetValue(ambient);
    ambientSpinner->SetDigits(3);
    ambientSpinner->SetIncrement(0.001);
    ambientSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_AMBIENT, evt.GetValue());
        controller->tryRedraw();
    });
    ambientSizer->Add(ambientSpinner);
    visbarSizer->Add(ambientSizer);


    auto enableControls = [=](int renderIdx) {
        cutoffSpinner->Enable(renderIdx == 0);
        particleSizeSpinner->Enable(renderIdx == 0);
        grayscaleBox->Enable(renderIdx == 0);
        keyBox->Enable(renderIdx == 0);
        aaBox->Enable(renderIdx == 0);
        smoothBox->Enable(renderIdx == 0);
        levelSpinner->Enable(renderIdx == 2);
        sunlightSpinner->Enable(renderIdx == 2);
        ambientSpinner->Enable(renderIdx == 2);
    };
    enableControls(0);

    particleButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        controller->setRenderer(makeAuto<ParticleRenderer>(gui));
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
        IScheduler& scheduler = *ThreadPool::getGlobalInstance();
        controller->setRenderer(makeAuto<RayTracer>(scheduler, gui));
        enableControls(2);
    });

    visbarSizer->AddSpacer(16);
    quantityPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, textSize);
    visbarSizer->Add(quantityPanel);
    quantityPanelSizer = visbarSizer;

    return visbarSizer;
}

void MainWindow::updateCutoff(const double cutoff) {
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

wxBoxSizer* MainWindow::createPlotBar() {
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    probe = new ParticleProbe(this, wxSize(300, 155));
    sidebarSizer->Add(probe.get(), 1, wxALIGN_TOP);
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

    if (flags.has(PlotEnum::PARTICLE_SFD)) {
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

                const Size largestRemnantIdx = std::distance(m.begin(), std::max_element(m.begin(), m.end()));
                const Float w = getLength(omega[largestRemnantIdx]);
                if (w == 0._f) {
                    return 0._f;
                } else {
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
    }

    if (flags.has(PlotEnum::SELECTED_PARTICLE)) {
        selectedParticlePlot = makeLocking<SelectedParticlePlot>();
        data.plot = selectedParticlePlot;
        plots.push(data.plot);
        data.color = Rgba(wxColour(255, 255, 255));
        list->push(data);
    } else {
        selectedParticlePlot.reset();
    }


    PlotView* firstPlot = new PlotView(this, wxSize(300, 200), wxSize(10, 10), list, 0, false);
    sidebarSizer->Add(firstPlot, 1, wxALIGN_TOP);
    sidebarSizer->AddSpacer(5);

    PlotView* secondPlot =
        new PlotView(this, wxSize(300, 200), wxSize(10, 10), list, list->size() == 1 ? 0 : 1, false);
    sidebarSizer->Add(secondPlot, 1, wxALIGN_TOP);

    /*wxButton* settingsButton = new wxButton(this, wxID_ANY, "Show material");
    sidebarSizer->Add(settingsButton);*/

    return sidebarSizer;
}

wxBoxSizer* MainWindow::createStatusBar() {
    wxBoxSizer* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    wxSize size;
    size.x = gui.get<int>(GuiSettingsId::VIEW_WIDTH) / 2 - 2;
    size.y = 215;

    status = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, size, wxTE_READONLY | wxTE_MULTILINE);
    wxTextAttr attr;
    attr.SetFontSize(9);
    status->SetDefaultStyle(attr);
    status->ChangeValue("No log");

    errors = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, size, wxTE_READONLY | wxTE_MULTILINE);
    attr.SetTextColour(*wxGREEN);
    errors->SetDefaultStyle(attr);
    errors->ChangeValue("No problems detected");
    errors->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& evt) {
        wxPoint pos = evt.GetPosition();
        long index;
        wxTextCtrlHitTestResult result = errors->HitTest(pos, &index);
        if (result != wxTE_HT_ON_TEXT) {
            return;
        }

        const std::string text(errors->GetValue().mb_str());
        if (index >= 0 && index < int(text.size()) && std::isdigit(text[index])) {
            // this relies on leading '#' and trailing ':'; see below
            int startIdx = index;
            while (startIdx >= 0 && std::isdigit(text[startIdx])) {
                startIdx--;
            }
            int endIdx = index;
            while (endIdx < int(text.size()) && std::isdigit(text[endIdx])) {
                endIdx++;
            }
            if (startIdx >= 0 && text[startIdx] == '#' && endIdx < int(text.size()) && text[endIdx] == ':') {
                const int particleIdx = std::stoi(text.substr(startIdx + 1, endIdx - startIdx - 1));
                controller->setSelectedParticle(particleIdx);
                pane->Refresh();
            }
        }
    });

    statusSizer->Add(status);
    statusSizer->AddSpacer(4);
    statusSizer->Add(errors);
    return statusSizer;
}

void MainWindow::setColorizer(const Size idx) {
    // do this even if idx==selectedIdx, we might change the colorizerList (weird behavior, but it will do for
    // now)
    controller->setColorizer(colorizerList[idx]);
    if (idx == selectedIdx) {
        return;
    }
    this->replaceQuantityBar(idx);
    selectedIdx = idx;
}

void MainWindow::addComponentIdBar(wxWindow* parent, wxSizer* sizer, SharedPtr<IColorizer> colorizer) {
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
    wxStaticText* text =
        new wxStaticText(parent, wxID_ANY, "Index", wxDefaultPosition, wxSize(textSize.x - 30, -1));
    highlightSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    wxSpinCtrl* highlightIndex = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, spinnerSizer);
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

void MainWindow::replaceQuantityBar(const Size idx) {
    quantityPanel->Destroy();

    quantityPanel = new wxPanel(this, wxID_ANY);
    // so far only needed for component id, so it is hacked like this
    SharedPtr<IColorizer> newColorizer = colorizerList[idx];
    /// \todo implemnet SharedPtr dynamicCast
    if (typeid(*newColorizer) == typeid(ComponentIdColorizer)) {
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


void MainWindow::setProgress(const float progress) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    gauge->SetValue(int(progress * 1000.f));
}

void MainWindow::runStarted() {
    for (auto plot : plots) {
        plot->clear();
    }
}

class LinesLogger : public ILogger {
private:
    Array<std::string> lines;

public:
    virtual void writeString(const std::string& s) override {
        lines.push(s);
    }

    void setText(wxTextCtrl* text) {
        text->Clear();
        // remove the last newline
        if (!lines.empty() && !lines.back().empty() && lines.back().back() == '\n') {
            lines.back() = lines.back().substr(0, lines.back().size() - 1);
        }
        for (const std::string& line : lines) {
            text->AppendText(line);
        }
    }
};

void MainWindow::onTimeStep(const Storage& storage, const Statistics& stats) {
    // this is called from run thread (NOT main thread)

    // limit the refresh rate to avoid blocking the main thread
    if (status && statusTimer.elapsed(TimerUnit::MILLISECOND) > 10) {
        // we get the data using CommonStatsLog (even though it's not really designed for it), in order to
        // avoid code duplication
        /// \todo how to access settings here??
        SharedPtr<LinesLogger> logger = makeShared<LinesLogger>();
        StandardLogWriter statsLog(logger, RunSettings::getDefaults());
        statsLog.write(storage, stats);
        // we have to modify wxTextCtrl from main thread!!
        executeOnMainThread([this, logger] { logger->setText(status); });
        statusTimer.restart();
    }

    pane->onTimeStep(storage, stats);

    if (selectedParticlePlot) {
        const Optional<Particle> particle = controller->getSelectedParticle();
        if (particle) {
            selectedParticlePlot->selectParticle(particle->getIndex());
        } else {
            selectedParticlePlot->selectParticle(NOTHING);
        }

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
    }
}

void MainWindow::onRunFailure(const DiagnosticsError& error, const Statistics& stats) {
    if (!errors) {
        return;
    }
    const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
    executeOnMainThread([this, error, t] {
        if (!errorReported) {
            errors->Clear(); // clear the "no problems" message
            errorReported = true;
        }
        errors->SetForegroundColour(*wxRED);
        std::string message = "t = " + std::to_string(t) + ":  " + error.description + "\n";
        Size counter = 0;
        for (auto idxAndMessage : error.offendingParticles) {
            message +=
                "  * particle #" + std::to_string(idxAndMessage.first) + ": " + idxAndMessage.second + "\n";
            if (counter++ >= 8) {
                // skip the rest, probably no point of listing ALL the particles
                message += "    ... " + std::to_string(error.offendingParticles.size()) + " total\n";
                break;
            }
        }
        errors->AppendText(message);
        if (errors->GetNumberOfLines() > 100) {
            // prevent displaying a huge log by deleting the first half
            const Size size = errors->GetLastPosition();
            errors->Remove(0, size / 2);
        }
    });
}

void MainWindow::setColorizerList(Array<SharedPtr<IColorizer>>&& colorizers) {
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

void MainWindow::setSelectedParticle(const Particle& particle, const Rgba color) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->update(particle, color);
}

void MainWindow::deselectParticle() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->clear();
}

wxSize MainWindow::getCanvasSize() const {
    return pane->GetSize();
}

void MainWindow::onClose(wxCloseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // veto the event, we will close the window ourselves
    if (!controller->isQuitting()) {
        evt.Veto();
        controller->quit();
    }
    // don't wait till it's closed so that we don't block the main thread
}

NAMESPACE_SPH_END
