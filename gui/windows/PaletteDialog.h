#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/UnorderedMap.h"
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

    UnorderedMap<std::string, Palette> paletteMap;

    Palette initial;
    Palette selected;

public:
    PalettePanel(wxWindow* parent, wxSize size, const Palette palette);

    void setPalette(const Palette& palette);

    Function<void(Palette)> onPaletteChanged;

private:
    void setDefaultPaletteList();

    void loadPalettes(const Path& path);

    void update();
};

NAMESPACE_SPH_END
