#pragma once

#include "gui/Palette.h"
#include "gui/Renderer.h"
#include "gui/Settings.h"
#include "objects/containers/Array.h"
#include "objects/containers/ArrayView.h"

#include <wx/panel.h>
#include <wx/timer.h>
#include <wx/wx.h>

NAMESPACE_SPH_BEGIN

class Storage;

class OrthoPane : public wxPanel, public Abstract::Renderer {
private:
    std::shared_ptr<Storage> storage;
    Array<int> displayedIdxs;
    QuantityKey quantity = QuantityKey::POSITIONS;
    Array<Vector> positions;
    Array<Color> colors;
    Palette palette;
    GuiSettings settings;
    wxPoint center = wxPoint(320, 240);
    wxPoint lastMousePosition;

public:
    OrthoPane(wxWindow* parent, const std::shared_ptr<Storage>& storage, const GuiSettings& settings);

    ~OrthoPane();

    virtual void draw(const std::shared_ptr<Storage>& storage) override;

    virtual void setQuantity(const QuantityKey key) override;

private:
    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void update();
};

NAMESPACE_SPH_END
