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
    wxSpinCtrl* cutoffSpinner = new wxSpinCtrl(this,
        wxID_ANY,
        std::to_string(gui.get<Float>(GuiSettingsId::ORTHO_CUTOFF)),
        wxDefaultPosition,
        wxSize(80, -1));
    cutoffSpinner->SetRange(0, 1000000);
    cutoffSpinner->Bind(wxEVT_SPINCTRL, [this, parent](wxSpinEvent& evt) {
        int cutoff = evt.GetPosition();
        GuiSettings modifiedGui = gui;
        modifiedGui.set(GuiSettingsId::ORTHO_CUTOFF, Float(cutoff));
        parent->setRenderer(makeAuto<ParticleRenderer>(modifiedGui));
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
    SharedPtr<Array<PlotData>> list = makeShared<Array<PlotData>>();

    TemporalPlot::Params params;
    params.segment = 1._f;
    params.minRangeY = 1.4_f;
    params.fixedRangeX = Interval{ 0._f, 10._f };
    params.shrinkY = false;
    params.period = 0.05_f;

    PlotData data;
    IntegralWrapper integral = makeAuto<TotalEnergy>();
    data.plot = makeLocking<TemporalPlot>(integral, params);
    plots.push(data.plot);
    data.color = Color(wxColour(240, 255, 80));
    list->push(data);

    integral = makeAuto<TotalKineticEnergy>();
    data.plot = makeLocking<TemporalPlot>(integral, params);
    plots.push(data.plot);
    data.color = Color(wxColour(200, 0, 0));
    list->push(data);

    integral = makeAuto<TotalInternalEnergy>();
    data.plot = makeLocking<TemporalPlot>(integral, params);
    plots.push(data.plot);
    data.color = Color(wxColour(255, 50, 50));
    list->push(data);

    integral = makeAuto<TotalMomentum>();
    params.minRangeY = 1.e6_f;
    data.plot = makeLocking<TemporalPlot>(integral, params);
    plots.push(data.plot);
    data.color = Color(wxColour(100, 200, 0));
    list->push(data);

    integral = makeAuto<TotalAngularMomentum>();
    data.plot = makeLocking<TemporalPlot>(integral, params);
    plots.push(data.plot);
    data.color = Color(wxColour(130, 80, 255));
    list->push(data);

    PlotView* energyPlot = new PlotView(this, wxSize(300, 200), wxSize(10, 10), list, 2, false);
    sidebarSizer->Add(energyPlot, 1, wxALIGN_TOP);
    sidebarSizer->AddSpacer(5);

    PlotView* secondPlot = new PlotView(this, wxSize(300, 200), wxSize(10, 10), list, 4, false);
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
