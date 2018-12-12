#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/Function.h"
#include <wx/frame.h>

class wxComboBox;

NAMESPACE_SPH_BEGIN

class PaletteCanvas;

class PaletteDialog : public wxFrame {
private:
    wxComboBox* paletteBox;

    PaletteCanvas* canvas;

    Function<void(Palette)> setPaletteCallback;

    FlatMap<std::string, Palette> paletteMap;

    Palette initial;

    Palette selected;

public:
    PaletteDialog(wxWindow* parent,
        wxSize size,
        const Palette initialPalette,
        Function<void(Palette)> setPalette);
    /*Optional<Palette> getSelectedPalette() const {
        return selected;
    }*/

private:
    void update();

    void setDefaultPaletteList();
};

NAMESPACE_SPH_END
