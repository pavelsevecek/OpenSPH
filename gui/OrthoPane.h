#pragma once

#include "gui/Palette.h"
#include "gui/Renderer.h"
#include "gui/Settings.h"
#include "objects/containers/ArrayView.h"
#include "objects/containers/BufferedArray.h"

#include <wx/panel.h>
#include <wx/timer.h>
#include <wx/wx.h>

NAMESPACE_SPH_BEGIN

class Storage;

class OrthoPane : public wxPanel, public Abstract::Renderer {
private:
    std::shared_ptr<Storage> storage;
    BufferedArray<Size> displayedIdxs;
    QuantityKey quantity = QuantityKey::POSITIONS;
    Array<Vector> positions;
    BufferedArray<Color> colors;
    Palette palette;
    GuiSettings settings;
    wxPoint center = wxPoint(320, 240);
    float fov;
    wxPoint lastMousePosition;
    wxTimer* refreshTimer;

public:
    OrthoPane(wxWindow* parent, const std::shared_ptr<Storage>& storage, const GuiSettings& settings);

    ~OrthoPane();

    virtual void draw(const std::shared_ptr<Storage>& storage) override;

    virtual void setQuantity(const QuantityKey key) override;

private:
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);

    void onTimer(wxTimerEvent& evt);

    void drawPalette(wxPaintDC& dc);

    void update();
};

NAMESPACE_SPH_END
