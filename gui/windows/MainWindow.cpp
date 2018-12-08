#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayTracer.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/ParticleProbe.h"
#include "gui/windows/PlotView.h"
#include "io/LogFile.h"
#include "sph/Diagnostics.h"
#include "thread/CheckFunction.h"
#include <fstream>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds {
    QUANTITY_BOX,
    RENDERER_BOX,
};

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
    wxBoxSizer* toolbarSizer = createToolbar(parent);
    sizer->Add(toolbarSizer);

    // everything below toolbar
    wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
    pane = alignedNew<OrthoPane>(this, parent, settings);
    mainSizer->Add(pane.get(), 4);
    mainSizer->AddSpacer(5);

    wxBoxSizer* sidebarSizer = createSidebar();
    mainSizer->Add(sidebarSizer);
    sizer->Add(mainSizer);

    /// \todo generalize window setup
    if (plugin) {
        plugin->create(this, sizer);
    } else {
        // status bar
        sizer->AddSpacer(5);
        wxBoxSizer* statusSizer = createStatusbar();
        sizer->Add(statusSizer);
    }

    this->SetSizer(sizer);
    this->Layout();

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


    static wxString fileDesc = "SPH state file (*.ssf)|*.ssf|Text file (*.txt)|*.txt";
    button = new wxButton(this, wxID_ANY, "Save");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent, this](wxCommandEvent& UNUSED(evt)) {
        wxFileDialog saveFileDialog(
            this, _("Save state file"), "", "", fileDesc, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveFileDialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        Path path(std::string(saveFileDialog.GetPath()));
        if (saveFileDialog.GetFilterIndex() == 0) {
            path.replaceExtension("ssf");
        } else {
            path.replaceExtension("txt");
        }
        parent->saveState(path);
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
    cutoffSpinner->Bind(wxEVT_SPINCTRL, [parent](wxSpinEvent& evt) {
        int cutoff = evt.GetPosition();
        // has to be generalized if perspective camera gets cutoff
        AutoPtr<ICamera> camera = parent->getCurrentCamera();
        if (RawPtr<OrthoCamera> ortho = dynamicCast<OrthoCamera>(camera.get())) {
            ortho->setCutoff(cutoff > 0 ? Optional<float>(cutoff) : NOTHING);
            parent->refresh(std::move(camera));
        }
    });
    toolbar->Add(cutoffSpinner);

    wxComboBox* rendererBox = new wxComboBox(this, int(ControlIds::RENDERER_BOX), "");
    rendererBox->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
    rendererBox->Append("Particles");
    rendererBox->Append("Mesh");
    rendererBox->Append("Surface");
    rendererBox->SetSelection(0);
    rendererBox->Bind(wxEVT_COMBOBOX, [this, rendererBox](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const int idx = rendererBox->GetSelection();
        IScheduler& scheduler = *ThreadPool::getGlobalInstance();
        switch (idx) {
        case 0:
            controller->setRenderer(makeAuto<ParticleRenderer>(gui));
            break;
        case 1:
            controller->setRenderer(makeAuto<MeshRenderer>(scheduler, gui));
            break;
        case 2:
            controller->setRenderer(makeAuto<RayTracer>(scheduler, gui));
            break;
        default:
            NOT_IMPLEMENTED;
        }
    });
    toolbar->Add(rendererBox);

    wxButton* resetView = new wxButton(this, wxID_ANY, "Reset view");
    resetView->Bind(wxEVT_BUTTON, [this, parent](wxCommandEvent& UNUSED(evt)) {
        pane->resetView();
        AutoPtr<ICamera> camera = parent->getCurrentCamera();
        camera->transform(AffineMatrix::identity());
        parent->refresh(std::move(camera));
    });
    toolbar->Add(resetView);

    wxButton* refresh = new wxButton(this, wxID_ANY, "Refresh");
    refresh->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->tryRedraw(); });
    toolbar->Add(refresh);

    gauge = new wxGauge(this, wxID_ANY, 1000);
    gauge->SetValue(0);
    gauge->SetMinSize(wxSize(300, -1));
    toolbar->AddSpacer(10);
    toolbar->Add(gauge, 0, wxALIGN_CENTER_VERTICAL);
    return toolbar;
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
        data.plot = makeLocking<SfdPlot>(NOTHING, 0._f);
        plots.push(data.plot);
        data.color = Rgba(wxColour(0, 190, 255));
        list->push(data);
    }

    if (flags.has(PlotEnum::CURRENT_SFD)) {
        Array<AutoPtr<IPlot>> multiplot;
        multiplot.emplaceBack(makeAuto<SfdPlot>(Post::ComponentConnectivity::OVERLAP, params.period));
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
        multiplot.emplaceBack(makeAuto<SfdPlot>(Post::ComponentConnectivity::ESCAPE_VELOCITY, params.period));
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

    return sidebarSizer;
}

wxBoxSizer* MainWindow::createStatusbar() {
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
        CommonStatsLog statsLog(logger, RunSettings::getDefaults());
        statsLog.write(storage, stats);
        // we have to modify wxTextCtrl from main thread!!
        executeOnMainThread([this, logger] { logger->setText(status); });
        statusTimer.restart();
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
    for (auto plot : plots) {
        plot->onTimeStep(storage, stats);
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
