#pragma once

#include "gui/Renderer.h"
#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "objects/containers/ArrayView.h"
#include "objects/containers/BufferedArray.h"

#include <wx/panel.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <mutex>

NAMESPACE_SPH_BEGIN

class Storage;
namespace Abstract {
    class Camera;
    class Element;
}

class OrthoPane : public wxPanel, public Abstract::Renderer {
private:
    std::shared_ptr<Storage> storage;

    struct Particle {
        Vector position;
        Color color;
    };
    BufferedArray<Particle> particles;
    QuantityIds quantity = QuantityIds::POSITIONS;
    bool particlesUpdated = false;

    std::unique_ptr<Abstract::Camera> camera;
    std::unique_ptr<Abstract::Element> element;
    GuiSettings settings;
    Point lastMousePosition;
    wxTimer* refreshTimer;
    wxBitmap bitmap;

    std::mutex mutex;

public:
    OrthoPane(wxWindow* parent, const std::shared_ptr<Storage>& storage, const GuiSettings& settings);

    ~OrthoPane();

    virtual void draw(const std::shared_ptr<Storage>& storage) override;

    virtual Bitmap getRender() const override;

    virtual void setQuantity(const QuantityIds key) override;

private:
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);

    void onTimer(wxTimerEvent& evt);

    void drawPalette(wxDC& dc);

    void update();
};

NAMESPACE_SPH_END
