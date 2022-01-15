#pragma once

#include "gui/objects/Palette.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/Function.h"
#include <wx/panel.h>

class wxCheckBox;
class wxButton;

NAMESPACE_SPH_BEGIN

class ComboBox;
class FloatTextCtrl;
class PaletteCanvas;
class PaletteEditor;

class PaletteSimpleWidget : public wxPanel {
private:
    PaletteCanvas* canvas;
    ComboBox* presetBox;
    FloatTextCtrl* lowerCtrl;
    FloatTextCtrl* upperCtrl;
    wxCheckBox* presetCheck;
    wxButton* defaultButton;

    UnorderedMap<String, Palette> presetMap;
    Palette defaultPalette;

public:
    PaletteSimpleWidget(wxWindow* parent, wxSize size, const Palette& palette, const Palette& defaultPalette);

    void setPalette(const Palette& palette, const Palette& defaultPalette);

    Palette getPalette() const;

    Function<void(Palette)> onPaletteChanged;

private:
    void setFromPresets();

    void setPaletteColors(const Palette& palette);
};

class PaletteAdvancedWidget : public wxPanel {
private:
    PaletteEditor* editor;

    ComboBox* presetBox;
    ComboBox* fileBox;

    UnorderedMap<String, Palette> fileMap;
    UnorderedMap<String, Palette> presetMap;

    Palette initialPalette; // palette when the panel was created
    Palette customPalette;  // customized palette

public:
    PaletteAdvancedWidget(wxWindow* parent, wxSize size, const Palette& palette);

    Function<void(const Palette& palette)> onPaletteChanged;

    const Palette& getPalette() const;
    const Palette& getInitialPalette() const;

private:
    void loadPalettes(const Path& path);

    void setFromPresets();

    void setFromFile();

    bool fileDialog();
};

NAMESPACE_SPH_END
