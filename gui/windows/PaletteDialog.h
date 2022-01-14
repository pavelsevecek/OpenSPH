#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/Function.h"
#include <wx/dialog.h>
#include <wx/panel.h>

class wxRadioBox;

NAMESPACE_SPH_BEGIN

class ComboBox;
class FloatTextCtrl;
class PaletteCanvas;
class PaletteEditor;

class PalettePanel : public wxPanel {
private:
    PaletteCanvas* canvas;
    FloatTextCtrl* lowerCtrl;
    FloatTextCtrl* upperCtrl;
    wxRadioBox* scaleBox;

    Palette initial;
    Palette selected;

public:
    PalettePanel(wxWindow* parent, wxSize size, const Palette& palette);

    void setPalette(const Palette& palette);

    Palette getPalette() const {
        return selected;
    }

    Function<void(Palette)> onPaletteChanged;

private:
    void update();
};

class PaletteSetup : public wxDialog {
private:
    PaletteEditor* editor;

    ComboBox* presetBox;
    ComboBox* fileBox;

    UnorderedMap<String, Palette> presetMap;
    UnorderedMap<String, Palette> fileMap;

    Palette initialPalette; // palette when the dialog opened
    Palette customPalette;  // customized palette
    Palette defaultPalette; // default palette for this colorizer

public:
    PaletteSetup(wxWindow* parent, wxSize size, const Palette& palette, const Palette& defaultPalette);

    void setPaletteChangedCallback(Function<void(const Palette& palette)> onPointsChanged);

    const Palette& getPalette() const;

private:
    void setDefaultPaletteList();

    void loadPalettes(const Path& path);

    void setFromPresets();

    void setFromFile();

    bool fileDialog();
};

NAMESPACE_SPH_END
