#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/Function.h"
#include <wx/frame.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class Pixel;
class ComboBox;
class PaletteViewCanvas;
class FloatTextCtrl;

void drawPalette(wxDC& dc, const wxPoint origin, const wxSize size, const Palette& palette);

class ColorLutPanel : public wxPanel {
private:
    ComboBox* paletteBox;

    PaletteViewCanvas* canvas;
    FloatTextCtrl* lowerCtrl;
    FloatTextCtrl* upperCtrl;

    UnorderedMap<String, Palette> paletteMap;

    ColorLut initial;
    ColorLut selected;

public:
    ColorLutPanel(wxWindow* parent, wxSize size, const ColorLut lut);

    void setLut(const ColorLut& newLut);

    Function<void(ColorLut)> onLutChanged;

private:
    void setDefaultPaletteList();

    void loadPalettes(const Path& path);

    void update();
};

NAMESPACE_SPH_END
