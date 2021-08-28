#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/Function.h"
#include <wx/frame.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class Pixel;
class ComboBox;
class FloatTextCtrl;

void drawPalette(wxDC& dc, const wxPoint origin, const wxSize size, const Palette& palette);

class PaletteViewCanvas : public wxPanel {
protected:
    ColorLut lut;

public:
    PaletteViewCanvas(wxWindow* parent, const ColorLut lut)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
        , lut(lut) {
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteViewCanvas::onPaint));
        this->SetMinSize(wxSize(320, 100));
    }

    void setPalette(const Palette& palette) {
        lut.setPalette(palette);
        this->Refresh();
    }

private:
    void onPaint(wxPaintEvent& evt);
};

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
