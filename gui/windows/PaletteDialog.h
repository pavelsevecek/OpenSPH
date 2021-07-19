#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/Function.h"
#include <wx/frame.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class ComboBox;
class PaletteCanvas;

class PalettePanel : public wxPanel {
private:
    ComboBox* paletteBox;

    PaletteCanvas* canvas;

    Function<void(Palette)> setPaletteCallback;

    FlatMap<std::string, Palette> paletteMap;

    Palette initial;

    Palette selected;

public:
    PalettePanel(wxWindow* parent, wxSize size, const Palette palette, Function<void(Palette)> setPalette);

private:
    void update();

    void setDefaultPaletteList();
};

class PaletteDialog : public wxFrame {
public:
    PaletteDialog(wxWindow* parent, wxSize size, const Palette palette, Function<void(Palette)> setPalette)
        : wxFrame(parent, wxID_ANY, "Palette Dialog", wxGetMousePosition(), size) {
        new PalettePanel(this, size, palette, setPalette);
    }
};

NAMESPACE_SPH_END
