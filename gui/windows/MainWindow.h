#pragma once

#include "gui/Settings.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/LockingPtr.h"
#include <wx/frame.h>

class wxComboBox;
class wxBoxSizer;
class wxGauge;
class wxCheckBox;

NAMESPACE_SPH_BEGIN

class IColorizer;
class IPlot;
class Controller;
class OrthoPane;
class ParticleProbe;
class PlotView;
class Particle;
class Color;
class Statistics;
class Storage;

/// Main frame of the application. Run is coupled with the window; currently there can only be one window and
/// one run at the same time. Run is ended when user closes the window.
class MainWindow : public wxFrame {
private:
    /// Parent control object
    RawPtr<Controller> controller;

    /// Drawing pane
    /// \todo used some virtual base class instead of directly orthopane
    RawPtr<OrthoPane> pane;

    RawPtr<ParticleProbe> probe;

    Array<LockingPtr<IPlot>> plots;

    /// Additional wx controls
    wxComboBox* quantityBox;
    Size selectedIdx = 0;
    wxGauge* gauge;
    wxCheckBox* shadingBox;

    Array<SharedPtr<IColorizer>> colorizerList;

public:
    MainWindow(Controller* controller, const GuiSettings& guiSettings);

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

    void onComboBox(wxCommandEvent& evt);

    void onClose(wxCloseEvent& evt);
};


NAMESPACE_SPH_END
