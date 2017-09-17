#pragma once

#include "gui/ArcBall.h"
#include "gui/Settings.h"
#include "gui/objects/Point.h"
#include <wx/panel.h>

class wxTimer;

NAMESPACE_SPH_BEGIN

class Controller;

class OrthoPane : public wxPanel {
private:
    Controller* controller;

    /// Helper for rotation
    ArcBall arcBall;

    struct {
        /// Cached last mouse position when dragging the window
        Point position;

        /// Camera rotation matrix when dragging started.
        AffineMatrix initialMatrix = AffineMatrix::identity();
    } dragging;

    struct {
        Size lastIdx;
    } particle;

public:
    OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui);

    ~OrthoPane();

    void resetView() {
        dragging.initialMatrix = AffineMatrix::identity();
    }

private:
    /// wx event handlers
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onRightDown(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);
};

NAMESPACE_SPH_END
