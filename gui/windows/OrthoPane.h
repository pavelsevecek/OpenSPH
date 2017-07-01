#pragma once

#include "gui/Settings.h"
#include "gui/objects/Point.h"
#include <wx/panel.h>

class wxTimer;

NAMESPACE_SPH_BEGIN

class Controller;

class OrthoPane : public wxPanel {
private:
    Controller* controller;

    /// Cached mouse position when dragging the window
    struct {
        Point position;
    } dragging;

    struct {
        Size lastIdx;
    } particle;

public:
    OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui);

    ~OrthoPane();

private:
    /// wx event handlers
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);
};

NAMESPACE_SPH_END
