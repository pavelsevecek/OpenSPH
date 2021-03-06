#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/Function.h"
#include <wx/frame.h>

NAMESPACE_SPH_BEGIN

class ComboBox;
class PaletteCanvas;

class PaletteDialog : public wxFrame {
private:
    ComboBox* paletteBox;

    PaletteCanvas* canvas;

    Function<void(Palette)> setPaletteCallback;

    FlatMap<std::string, Palette> paletteMap;

    Palette initial;

    Palette selected;

public:
    PaletteDialog(wxWindow* parent, wxSize size, const Palette palette, Function<void(Palette)> setPalette);

private:
    void update();

    void setDefaultPaletteList();
};

NAMESPACE_SPH_END
