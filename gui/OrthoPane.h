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
    ArrayView<Vector> positions;
    Palette palette;
    GuiSettings settings;

    void OnPaint(wxPaintEvent& evt);

public:
    OrthoPane(wxWindow* parent, const GuiSettings& settings);

    ~OrthoPane();

    virtual void draw(const std::shared_ptr<Storage>& storage) override;
};

NAMESPACE_SPH_END
