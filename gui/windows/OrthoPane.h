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
        Pixel lastPosition;

    } dragging;

    struct {
        Vector pivot = Vector(0._f);

        /// Current transform applied on camera while orbiting.
        AffineMatrix matrix = AffineMatrix::identity();
    } orbit;

    struct {
        Optional<Size> lastIdx;
    } particle;

public:
    OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui);

    ~OrthoPane();

    virtual ICamera& getCamera() override {
        return *camera;
    }

    virtual void resetView() override;

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override;

private:
    /// wx event handlers
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onRightDown(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onDoubleClick(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);

    void onResize(wxSizeEvent& evt);
};

NAMESPACE_SPH_END
