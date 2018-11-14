#pragma once

#include "gui/ArcBall.h"
#include "gui/Settings.h"
#include "gui/objects/Point.h"
#include "gui/windows/IGraphicsPane.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

class Controller;
class ICamera;

class OrthoPane : public IGraphicsPane {
private:
    Controller* controller;

    /// Helper for rotation
    ArcBall arcBall;

    AutoPtr<ICamera> camera;

    struct {
        /// Cached last mouse position when dragging the window
        Pixel position;

        /// Camera rotation matrix when dragging started.
        AffineMatrix initialMatrix = AffineMatrix::identity();
    } dragging;

    struct {
        Optional<Size> lastIdx;
    } particle;

public:
    OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui);

    ~OrthoPane();

    virtual void resetView() override;

private:
    /// wx event handlers
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onRightDown(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);
};

NAMESPACE_SPH_END
