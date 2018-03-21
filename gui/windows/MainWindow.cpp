#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/SurfaceRenderer.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/ParticleProbe.h"
#include "gui/windows/PlotView.h"
#include "thread/CheckFunction.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds { QUANTITY_BOX };

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
    /// Sinces range getters are not virtual, we have to update ranges manually.
    void syncRanges() {
        if (currentPlot) {
            ranges.x = currentPlot->rangeX();
            ranges.y = currentPlot->rangeY();
        }
    }
};

MainWindow::MainWindow(Controller* parent, const GuiSettings& settings)
    : controller(parent)
    , gui(settings) {
    // create the frame
    std::string title = settings.get<std::string>(GuiSettingsId::WINDOW_TITLE);
    wxSize size(
        settings.get<int>(GuiSettingsId::WINDOW_WIDTH), settings.get<int>(GuiSettingsId::WINDOW_HEIGHT));
    this->Create(nullptr, wxID_ANY, title.c_str(), wxDefaultPosition, size);

    // toolbar
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* toolbarSizer = createToolbar(parent);
    sizer->Add(toolbarSizer);

    // everything below toolbar
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    pane = new OrthoPane(this, parent, settings);
    mainSizer->Add(pane.get(), 1, wxEXPAND);
    mainSizer->AddSpacer(5);

    wxBoxSizer* sidebarSizer = createSidebar();
    mainSizer->Add(sidebarSizer);

    sizer->Add(mainSizer);

    this->SetSizer(sizer);

    // connect event handlers
    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainWindow::onClose));
}

wxBoxSizer* MainWindow::createToolbar(Controller* parent) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxBoxSizer* toolbar = new wxBoxSizer(wxHORIZONTAL);

    wxButton* button = new wxButton(this, wxID_ANY, "Start");
    button->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->restart(); });
    toolbar->Add(button);

    button = new wxButton(this, wxID_ANY, "Pause");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->pause(); });

    button = new wxButton(this, wxID_ANY, "Stop");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->stop(); });


    static wxString fileDesc = "SPH state files (*.ssf)|*.ssf";
    button = new wxButton(this, wxID_ANY, "Save");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent, this](wxCommandEvent& UNUSED(evt)) {
        wxFileDialog saveFileDialog(
            this, _("Save state file"), "", "", fileDesc, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        const std::string path(saveFileDialog.GetPath());
        parent->saveState(Path(path).replaceExtension("ssf"));
    });

    button = new wxButton(this, wxID_ANY, "Load");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent, this](wxCommandEvent& UNUSED(evt)) {
        wxFileDialog loadFileDialog(
            this, _("Load state file"), "", "", fileDesc, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (loadFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        const std::string path(loadFileDialog.GetPath());
        parent->loadState(Path(path).replaceExtension("ssf"));
    });

    quantityBox = new wxComboBox(this, int(ControlIds::QUANTITY_BOX), "");
    quantityBox->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
    quantityBox->SetSelection(0);
    quantityBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const int idx = quantityBox->GetSelection();
        controller->setColorizer(colorizerList[idx]);
        selectedIdx = idx;
    });
    toolbar->Add(quantityBox);

    toolbar->Add(new wxStaticText(this, wxID_ANY, "Cutoff"), 0, wxALIGN_CENTER_VERTICAL);
    const Float cutoff = gui.get<Float>(GuiSettingsId::ORTHO_CUTOFF);
    wxSpinCtrl* cutoffSpinner = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1));
    cutoffSpinner->SetRange(0, 1000000);
    cutoffSpinner->SetValue(int(cutoff));
    cutoffSpinner->Bind(wxEVT_SPINCTRL, [this, parent](wxSpinEvent& evt) {
        int cutoff = evt.GetPosition();
        GuiSettings modifiedGui = gui;
        /// \todo this is horrible and needs refactoring
        modifiedGui.set(GuiSettingsId::ORTHO_CUTOFF, Float(cutoff));
        parent->setRenderer(makeAuto<ParticleRenderer>(modifiedGui));
        parent->getParams().set<Float>(GuiSettingsId::ORTHO_CUTOFF, cutoff);

    });
    toolbar->Add(cutoffSpinner);

    wxCheckBox* surfaceBox = new wxCheckBox(this, wxID_ANY, "Show surface");
    surfaceBox->Bind(wxEVT_CHECKBOX, [surfaceBox, this](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const bool checked = surfaceBox->GetValue();
        if (checked) {
            controller->setRenderer(makeAuto<SurfaceRenderer>(gui));
        } else {
            controller->setRenderer(makeAuto<ParticleRenderer>(gui));
        }
        surfaceBox->Refresh();
    });
    toolbar->Add(surfaceBox, 0, wxALIGN_CENTER_VERTICAL);

    wxButton* resetView = new wxButton(this, wxID_ANY, "Reset view");
    resetView->Bind(wxEVT_BUTTON, [this, parent](wxCommandEvent& UNUSED(evt)) {
        SharedPtr<ICamera> camera = parent->getCurrentCamera();
        camera->transform(AffineMatrix::identity());
        pane->resetView();
        // parent->tryRedraw();
    });
    toolbar->Add(resetView);

    wxButton* refresh = new wxButton(this, wxID_ANY, "Refresh");
    refresh->Bind(wxEVT_BUTTON, [this, parent](wxCommandEvent& UNUSED(evt)) { parent->tryRedraw(); });
    toolbar->Add(refresh);

    gauge = new wxGauge(this, wxID_ANY, 1000);
    gauge->SetValue(0);
    gauge->SetMinSize(wxSize(300, -1));
    toolbar->AddSpacer(10);
    toolbar->Add(gauge, 0, wxALIGN_CENTER_VERTICAL);
    return toolbar;
}

wxBoxSizer* MainWindow::createSidebar() {
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
    params.period = 1._f;

    PlotData data;
    IntegralWrapper integral;
    Flags<PlotEnum> flags = gui.getFlags<PlotEnum>(GuiSettingsId::PLOT_INTEGRALS);

    /// \todo this plots should really be set somewhere outside of main window, they are problem-specific
    if (flags.has(PlotEnum::TOTAL_ENERGY)) {
        integral = makeAuto<TotalEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Color(wxColour(240, 255, 80));
        list->push(data);
    }

    if (flags.has(PlotEnum::KINETIC_ENERGY)) {
        integral = makeAuto<TotalKineticEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Color(wxColour(200, 0, 0));
        list->push(data);
    }

    if (flags.has(PlotEnum::INTERNAL_ENERGY)) {
        integral = makeAuto<TotalInternalEnergy>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Color(wxColour(255, 50, 50));
        list->push(data);
    }

    if (flags.has(PlotEnum::TOTAL_MOMENTUM)) {
        integral = makeAuto<TotalMomentum>();
        params.minRangeY = 1.e6_f;
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Color(wxColour(100, 200, 0));
        list->push(data);
    }

    if (flags.has(PlotEnum::TOTAL_ANGULAR_MOMENTUM)) {
        integral = makeAuto<TotalAngularMomentum>();
        data.plot = makeLocking<TemporalPlot>(integral, params);
        plots.push(data.plot);
        data.color = Color(wxColour(130, 80, 255));
        list->push(data);
    }

    if (flags.has(PlotEnum::SIZE_FREQUENCY_DISTRIBUTION)) {
        data.plot = makeLocking<SfdPlot>();
        plots.push(data.plot);
        data.color = Color(wxColour(0, 190, 255));
        list->push(data);
    }

    if (flags.has(PlotEnum::SELECTED_PARTICLE)) {
        selectedParticlePlot = makeLocking<SelectedParticlePlot>();
        data.plot = selectedParticlePlot;
        plots.push(data.plot);
        data.color = Color(wxColour(255, 255, 255));
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

    return sidebarSizer;
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

void MainWindow::onTimeStep(const Storage& storage, const Statistics& stats) {
    if (selectedParticlePlot) {
        selectedParticlePlot->selectParticle(controller->getSelectedParticle());

        SharedPtr<IColorizer> colorizer = controller->getCurrentColorizer();
        // we need validity of arrayrefs only for the duration of this function, so weak reference is OK
        colorizer->initialize(storage, RefEnum::WEAK);
        selectedParticlePlot->setColorizer(colorizer);
    }
    for (auto plot : plots) {
        plot->onTimeStep(storage, stats);
    }
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

void MainWindow::setSelectedParticle(const Particle& particle, const Color color) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->update(particle, color);
}

void MainWindow::deselectParticle() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->clear();
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
