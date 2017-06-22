#pragma once

#include "gui/Renderer.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "objects/containers/ArrayView.h"
#include "objects/containers/BufferedArray.h"
#include "quantities/Particle.h"

#include <mutex>
#include <wx/panel.h>
#include <wx/wx.h>

class wxTimer;

NAMESPACE_SPH_BEGIN

class Storage;
class Controller;

namespace Abstract {
    class Camera;
    class Element;
}


class OrthoRenderer : public Abstract::Renderer {
public:
    /// Can only be called from main thread
    virtual Bitmap render(ArrayView<const Vector> positions,
        const Abstract::Element& element,
        const Abstract::Camera& camera,
        const RenderParams& params,
        Statistics& stats) const override;

private:
    void drawPalette(wxDC& dc, const Palette& palette) const;
};

class OrthoPane : public wxPanel {
private:
    Controller* controller;

    /// Cached mouse position when dragging the window
    struct {
        Point position;
    } dragging;

    struct {
        Optional<Particle> particle;
    } selected;

    /// Current camera of the view. The object is shared with parent model.
    AutoPtr<Abstract::Camera> camera;

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
