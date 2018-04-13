#pragma once

#include "gui/Settings.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/LockingPtr.h"
#include "system/Settings.h"
#include <wx/frame.h>

class wxComboBox;
class wxBoxSizer;
class wxGauge;
class wxCheckBox;

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
class Color;
class Statistics;
class Storage;
class SelectedParticlePlot;

/// Main frame of the application. Run is coupled with the window; currently there can only be one window and
/// one run at the same time. Run is ended when user closes the window.
class MainWindow : public wxFrame {
private:
    /// Parent control object
    RawPtr<Controller> controller;

    /// Gui settings
    GuiSettings gui;

    /// Drawing pane (owned by wxWidgets)
    RawPtr<IGraphicsPane> pane;

    RawPtr<ParticleProbe> probe;

    Array<LockingPtr<IPlot>> plots;

    LockingPtr<SelectedParticlePlot> selectedParticlePlot;

    /// Additional wx controls
    wxComboBox* quantityBox;
    Size selectedIdx = 0;
    wxGauge* gauge;
    wxCheckBox* shadingBox;

    /// Colorizers corresponding to the items in combobox
    Array<SharedPtr<IColorizer>> colorizerList;

public:
    MainWindow(Controller* controller, const GuiSettings& guiSettings, RawPtr<IPluginControls> plugin);

    void runStarted();

    void onTimeStep(const Storage& storage, const Statistics& stats);

    void setProgress(const float progress);

    void setColorizerList(Array<SharedPtr<IColorizer>>&& colorizers);

    void setSelectedParticle(const Particle& particle, const Color color);

    void deselectParticle();

private:
    wxBoxSizer* createToolbar(Controller* parent);

    wxBoxSizer* createSidebar();

    /// wx event handlers

    void onClose(wxCloseEvent& evt);
};


NAMESPACE_SPH_END
