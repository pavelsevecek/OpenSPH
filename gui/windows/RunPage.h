#pragma once

#include "gui/Settings.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/LockingPtr.h"
#include "system/Settings.h"
#include "system/Timer.h"
#include <wx/panel.h>

class wxComboBox;
class wxBoxSizer;
class wxGauge;
class wxCheckBox;
class wxTextCtrl;
class wxPanel;
class wxDialog;

class wxAuiManager;
class wxAuiNotebookEvent;

NAMESPACE_SPH_BEGIN

class IColorizer;
class IGraphicsPane;
class IPlot;
class IPluginControls;
class Controller;
class OrthoPane;
class ParticleProbe;
class PlotView;
class Particle;
class Rgba;
class Statistics;
class Storage;
struct DiagnosticsError;
class SelectedParticlePlot;
class TimeLine;
class ProgressPanel;

/// \brief Main frame of the application.
///
/// Run is coupled with the window; currently there can only be one window and one run at the same time. Run
/// is ended when user closes the window.
class RunPage : public wxPanel {
private:
    /// Parent control object
    RawPtr<Controller> controller;

    AutoPtr<wxAuiManager> manager;

    /// Gui settings
    GuiSettings& gui;

    /// Drawing pane (owned by wxWidgets)
    RawPtr<IGraphicsPane> pane;

    RawPtr<ParticleProbe> probe;

    Array<LockingPtr<IPlot>> plots;
    PlotView* firstPlot = nullptr;
    PlotView* secondPlot = nullptr;


    LockingPtr<SelectedParticlePlot> selectedParticlePlot;

    wxTextCtrl* statsText = nullptr;
    Timer statsTimer;

    wxDialog* waitingDialog = nullptr;

    /// Additional wx controls
    wxComboBox* quantityBox;
    Size selectedIdx = 0;
    wxPanel* quantityPanel;
    wxSizer* quantityPanelSizer;

    TimeLine* timelineBar;
    ProgressPanel* progressBar;
    wxPanel* statsBar;

    /// Colorizers corresponding to the items in combobox
    Array<SharedPtr<IColorizer>> colorizerList;

public:
    RunPage(wxWindow* window, Controller* controller, GuiSettings& guiSettings);

    ~RunPage();

    void refresh();

    void showTimeLine(const bool show);

    void runStarted(const Storage& storage, const Path& path);

    void onTimeStep(const Storage& storage, const Statistics& stats);

    void onRunEnd();

    // false means close has been veto'd
    bool close();

    void setProgress(const Statistics& stats);

    void newPhase(const std::string& className, const std::string& instanceName);

    void setColorizerList(Array<SharedPtr<IColorizer>>&& colorizers);

    void setSelectedParticle(const Particle& particle, const Rgba color);

    void deselectParticle();

    wxSize getCanvasSize() const;

private:
    /// Toolbar on the top, containing buttons for controlling the run.
    // wxPanel* createToolBar();

    /// Panel on the right, with plots and particle info
    wxPanel* createPlotBar();

    /// Panel on the left, with visualization controls
    wxPanel* createVisBar();

    /// Panel on the right, with run statistics and error reporting
    wxPanel* createStatsBar();

    wxWindow* createParticleBox(wxPanel* parent);
    wxWindow* createRaytracerBox(wxPanel* parent);
    wxWindow* createContourBox(wxPanel* parent);


    void makeStatsText(const Size particleCnt, const Statistics& stats);

    void setColorizer(const Size idx);

    void replaceQuantityBar(const Size idx);

    void addComponentIdBar(wxWindow* parent, wxSizer* sizer, SharedPtr<IColorizer> colorizer);

    void updateCutoff(const double cutoff);
};


NAMESPACE_SPH_END
