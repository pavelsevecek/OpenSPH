#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Colorizer.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/ParticleProbe.h"
#include "gui/windows/PlotView.h"
#include "thread/CheckFunction.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/statline.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds { QUANTITY_BOX };

MainWindow::MainWindow(Controller* parent, const GuiSettings& settings)
    : controller(parent) {
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
    Connect(wxEVT_COMBOBOX, wxCommandEventHandler(MainWindow::onComboBox));
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
    toolbar->Add(quantityBox);
    gauge = new wxGauge(this, wxID_ANY, 1000);
    gauge->SetValue(0);
    gauge->SetMinSize(wxSize(300, -1));
    toolbar->AddSpacer(200);
    toolbar->Add(gauge, 0, wxALIGN_CENTER_VERTICAL);
    return toolbar;
}

wxBoxSizer* MainWindow::createSidebar() {
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    probe = new ParticleProbe(this, wxSize(300, 155));
    sidebarSizer->Add(probe.get(), 1, wxALIGN_TOP);
    sidebarSizer->AddSpacer(5);

    PlotView* energyPlot = new PlotView(
        this, wxSize(300, 200), wxSize(10, 10), makeAuto<TotalInternalEnergy>(), Color(230, 130, 10));
    sidebarSizer->Add(energyPlot, 1, wxALIGN_TOP);
    plots.push(energyPlot);
    sidebarSizer->AddSpacer(5);

    PlotView* secondPlot = new PlotView(
        this, wxSize(300, 200), wxSize(10, 10), makeAuto<TotalAngularMomentum>(), Color(130, 80, 255));
    sidebarSizer->Add(secondPlot, 1, wxALIGN_TOP);
    plots.push(secondPlot);

    return sidebarSizer;
}

void MainWindow::setProgress(const float progress) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    gauge->SetValue(int(progress * 1000.f));
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

void MainWindow::onComboBox(wxCommandEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const int idx = quantityBox->GetSelection();
    controller->setColorizer(colorizerList[idx]);
    selectedIdx = idx;
}

NAMESPACE_SPH_END
