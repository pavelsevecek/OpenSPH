#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"

#include <wx/panel.h>

class wxAuiManager;

NAMESPACE_SPH_BEGIN

class PaletteEditor : public wxPanel {
private:
    Palette palette;
    // copied-out palette points, must be kept in sync
    Array<Palette::Point> points;

    Optional<Size> active;
    bool enabled = true;

public:
    PaletteEditor(wxWindow* parent, const wxSize size, const Palette& palette);

    void setPalette(const Palette& newPalette);

    void setPaletteColors(const Palette& newPalette);

    const Palette& getPalette() const {
        return palette;
    }

    void enable(const bool value);

    Function<void(const Palette& palette)> onPaletteChangedByUser;

private:
    void updatePaletteFromPoints(const bool notify);

    void onPaint(wxPaintEvent& evt);

    Optional<Size> lock(const int x) const;

    float windowToPoint(const int x) const;

    int pointToWindow(const float x) const;

    void onMouseMotion(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onLeftDown(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onDoubleClick(wxMouseEvent& evt);
};

NAMESPACE_SPH_END
