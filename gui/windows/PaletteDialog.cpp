#include "gui/windows/PaletteDialog.h"
#include "gui/Factory.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/Spectrum.h"
#include "gui/windows/PaletteEditor.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include "post/Plot.h"
#include "post/Point.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

class PaletteCanvas : public wxPanel {
private:
    Palette palette;

public:
    PaletteCanvas(wxWindow* parent, const Palette palette)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
        , palette(palette) {
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteCanvas::onPaint));
        this->SetMinSize(wxSize(300, 80));
    }

    void setPalette(const Palette newPalette) {
        palette = newPalette;
        this->Refresh();
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        wxFont font = wxSystemSettings::GetFont(wxSystemFont::wxSYS_DEFAULT_GUI_FONT);
        font.SetPointSize(10);
        SPH_ASSERT(font.IsOk());
        dc.SetFont(font);
        WxRenderContext context(dc);
        context.setFontSize(9);
        Rgba background(dc.GetBackground().GetColour());
        drawPalette(context, Pixel(10, 10), Pixel(280, 40), palette, background.inverse());
    }
};

PalettePanel::PalettePanel(wxWindow* parent, wxSize size, const Palette& palette)
    : wxPanel(parent, wxID_ANY)
    , initial(palette)
    , selected(palette) {

    this->SetMinSize(size);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* rangeSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, "Range");

    wxStaticText* text = new wxStaticText(this, wxID_ANY, "From ");
    rangeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    lowerCtrl = new FloatTextCtrl(this, double(initial.getInterval().lower()));
    rangeSizer->Add(lowerCtrl);
    rangeSizer->AddSpacer(30);

    text = new wxStaticText(this, wxID_ANY, "To ");
    rangeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    upperCtrl = new FloatTextCtrl(this, double(initial.getInterval().upper()));
    rangeSizer->Add(upperCtrl);
    rangeSizer->SetMinSize(wxSize(300, -1));

    mainSizer->Add(rangeSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(10);

    canvas = new PaletteCanvas(this, initial);
    mainSizer->Add(canvas, 0, wxALIGN_CENTER_HORIZONTAL);

    this->SetSizerAndFit(mainSizer);

    this->Bind(wxEVT_RADIOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        const PaletteScale scale = PaletteScale(scaleBox->GetSelection());
        selected.setScale(scale);
        canvas->setPalette(selected);
        onPaletteChanged.callIfNotNull(selected);
    });
}

void PalettePanel::setPalette(const Palette& palette) {
    selected = initial = palette;
    // presetBox->SetSelection(0);
    canvas->setPalette(selected);
    lowerCtrl->setValue(initial.getInterval().lower());
    upperCtrl->setValue(initial.getInterval().upper());
    scaleBox->SetSelection(int(initial.getScale()));
}

static UnorderedMap<ExtColorizerId, String> PALETTE_ID_LIST = {
    { ColorizerId::VELOCITY, "Magnitude 1" },
    { QuantityId::DEVIATORIC_STRESS, "Magnitude 2" },
    { ColorizerId::TEMPERATURE, "Temperature" },
    { QuantityId::DAMAGE, "Grayscale" },
    { ColorizerId::MOVEMENT_DIRECTION, "Periodic" },
    { ColorizerId::DENSITY_PERTURBATION, "Diverging 1" },
    { QuantityId::DENSITY, "Diverging 2" },
    { QuantityId::VELOCITY_DIVERGENCE, "Diverging 3" },
    { QuantityId::ANGULAR_FREQUENCY, "Extremes" },
};

// some extra palettes
namespace Palettes {

const Palette GALAXY({ { 0.f, Rgba(0.f, 0.02f, 0.09f) },
                         { 0.25f, Rgba(0.4f, 0.106f, 0.38f) },
                         { 0.5f, Rgba(0.78f, 0.18f, 0.38f) },
                         { 0.75f, Rgba(0.91f, 0.56f, 0.81f) },
                         { 1.f, Rgba(0.29f, 0.69f, 0.93f) } },
    Interval(0.01f, 100.f),
    PaletteScale::LOGARITHMIC);

const Palette ACCRETION({ { 0.f, Rgba(0.43f, 0.70f, 1.f) },
                            { 0.2f, Rgba(0.5f, 0.5f, 0.5f) },
                            { 0.4f, Rgba(0.65f, 0.12f, 0.01f) },
                            { 0.6f, Rgba(0.79f, 0.38f, 0.02f) },
                            { 0.8f, Rgba(0.93f, 0.83f, 0.34f) },
                            { 1.f, Rgba(0.94f, 0.90f, 0.84f) } },
    Interval(0.01f, 100.f),
    PaletteScale::LOGARITHMIC);

const Palette STELLAR({ { 0.f, Rgba(1.f, 0.75f, 0.1f) },
                          { 0.333f, Rgba(0.75f, 0.25f, 0.1f) },
                          { 0.666f, Rgba(0.4f, 0.7f, 1.f) },
                          { 1.f, Rgba(0.2f, 0.4f, 0.8f) } },
    Interval(0.01f, 100.f),
    PaletteScale::LOGARITHMIC);

} // namespace Palettes


void PalettePanel::update() {
    // const int idx = presetBox->GetSelection();
    const Interval range = selected.getInterval();
    // selected = (paletteMap.begin() + idx)->value();
    selected.setInterval(range);
    canvas->setPalette(selected);
    onPaletteChanged.callIfNotNull(selected);
}

PaletteSetup::PaletteSetup(wxWindow* parent,
    wxSize size,
    const Palette& palette,
    const Palette& defaultPalette)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
    , initialPalette(palette)
    , customPalette(palette)
    , defaultPalette(defaultPalette) {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* rangeSizer = new wxStaticBoxSizer(wxHORIZONTAL, this, "Range");

    wxStaticText* text = new wxStaticText(this, wxID_ANY, "From ");
    rangeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    FloatTextCtrl* lowerCtrl = new FloatTextCtrl(this, double(palette.getInterval().lower()));
    rangeSizer->Add(lowerCtrl);
    rangeSizer->AddSpacer(30);

    text = new wxStaticText(this, wxID_ANY, "To ");
    rangeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    FloatTextCtrl* upperCtrl = new FloatTextCtrl(this, double(palette.getInterval().upper()));
    rangeSizer->Add(upperCtrl);
    rangeSizer->SetMinSize(wxSize(300, -1));

    mainSizer->Add(rangeSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(10);

    wxString scales[] = { "Linear", "Logarithmic", "Log-linear" };
    wxRadioBox* scaleBox =
        new wxRadioBox(this, wxID_ANY, "Scale", wxDefaultPosition, wxDefaultSize, 3, scales);
    scaleBox->SetSelection(int(palette.getScale()));
    scaleBox->SetMinSize(wxSize(300, -1));
    mainSizer->Add(scaleBox, 0, wxALIGN_CENTER_HORIZONTAL);

    editor = new PaletteEditor(this, wxSize(300, 40), palette);
    editor->onPaletteChangedByUser = [this](const Palette& newPalette) {
        customPalette = newPalette;
        onPaletteChanged.callIfNotNull(newPalette);
    };
    mainSizer->Add(editor, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(10);

    wxStaticBoxSizer* colorSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Colors");

    wxRadioButton* customRadio = new wxRadioButton(this, wxID_ANY, "Custom");
    colorSizer->Add(customRadio, 0, wxALIGN_CENTER_HORIZONTAL | wxRIGHT, 200);

    wxRadioButton* presetRadio = new wxRadioButton(this, wxID_ANY, "Presets ");
    colorSizer->Add(presetRadio, 0, wxALIGN_CENTER_HORIZONTAL | wxRIGHT, 200);

    wxStaticBoxSizer* presetSizer = new wxStaticBoxSizer(wxVERTICAL, this);

    wxRadioButton* defaultRadio =
        new wxRadioButton(this, wxID_ANY, "Default", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    presetSizer->Add(defaultRadio);

    wxRadioButton* listRadio = new wxRadioButton(this, wxID_ANY, "From list");
    presetSizer->Add(listRadio);

    presetBox = new ComboBox(this, "Select palette ...", 200);
    presetSizer->Add(presetBox, 0, wxALIGN_CENTER_HORIZONTAL);

    wxRadioButton* fileRadio = new wxRadioButton(this, wxID_ANY, "From file");
    presetSizer->Add(fileRadio);

    wxBoxSizer* fileSizer = new wxBoxSizer(wxHORIZONTAL);
    fileBox = new ComboBox(this, "Select palette ...");
    fileSizer->Add(fileBox);
    wxButton* loadButton = new wxButton(this, wxID_ANY, "Load...");
    fileSizer->Add(loadButton);

    presetSizer->Add(fileSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    colorSizer->Add(presetSizer, 0, wxALIGN_CENTER_HORIZONTAL);

    mainSizer->Add(colorSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(5);

    this->SetSizerAndFit(mainSizer);

    upperCtrl->onValueChanged = [this](const Float value) {
        /// \todo deduplicate
        Palette palette = editor->getPalette();
        const Float lower = palette.getInterval().lower();
        if (lower >= value) {
            return false;
        }
        palette.setInterval(Interval(lower, value));
        editor->setPalette(palette);
        onPaletteChanged.callIfNotNull(palette);
        return true;
    };
    lowerCtrl->onValueChanged = [this](const Float value) {
        Palette palette = editor->getPalette();
        const Float upper = palette.getInterval().upper();
        if (value >= upper) {
            return false;
        }
        if (palette.getScale() == PaletteScale::LOGARITHMIC && value <= 0.f) {
            return false;
        }
        palette.setInterval(Interval(value, upper));
        editor->setPalette(palette);
        onPaletteChanged.callIfNotNull(palette);
        return true;
    };

    this->Bind(wxEVT_RADIOBOX, [this, scaleBox](wxCommandEvent& UNUSED(evt)) {
        const PaletteScale scale = PaletteScale(scaleBox->GetSelection());
        Palette palette = editor->getPalette();
        palette.setScale(scale);
        editor->setPalette(palette);
        onPaletteChanged.callIfNotNull(palette);
    });

    loadButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->fileDialog(); });

    presetBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        this->setFromPresets();
        this->Refresh();
    });

    fileBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        this->setFromFile();
        this->Refresh();
    });

    auto update = [=] {
        bool useCustom = customRadio->GetValue();
        bool useDefault = !useCustom && defaultRadio->GetValue();
        bool usePresets = !useCustom && listRadio->GetValue();
        bool useFile = !useCustom && fileRadio->GetValue();
        editor->enable(useCustom);
        defaultRadio->Enable(!useCustom);
        listRadio->Enable(!useCustom);
        fileRadio->Enable(!useCustom);
        presetBox->Enable(usePresets);
        fileBox->Enable(useFile);
        loadButton->Enable(useFile);

        if (useCustom) {
            editor->setPaletteColors(customPalette);
        } else if (useDefault) {
            editor->setPaletteColors(defaultPalette);
        } else if (usePresets) {
            this->setFromPresets();
        } else if (useFile) {
            this->setFromFile();
        }

        onPaletteChanged.callIfNotNull(editor->getPalette());
        this->Refresh();
    };

    update();
    this->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) { update(); });

    this->setDefaultPaletteList();
}

void PaletteSetup::setFromPresets() {
    const int idx = presetBox->GetSelection();
    Palette selected = (presetMap.begin() + idx)->value();
    editor->setPaletteColors(selected);
}

void PaletteSetup::setFromFile() {
    if (fileBox->GetCount() == 0) {
        bool palettesLoaded = this->fileDialog();
        if (!palettesLoaded) {
            return;
        }
    }
    const int idx = fileBox->GetSelection();
    Palette selected = (presetMap.begin() + idx)->value();
    editor->setPaletteColors(selected);
}

bool PaletteSetup::fileDialog() {
    Optional<Path> path = doOpenFileDialog("Load palette", { { "Palette files", "csv" } });
    if (!path) {
        return false;
    }
    this->loadPalettes(path.value());
    return fileBox->GetCount() > 0;
}

const Palette& PaletteSetup::getPalette() const {
    return editor->getPalette();
}

const Palette& PaletteSetup::getInitialPalette() const {
    return initialPalette;
}

void PaletteSetup::setDefaultPaletteList() {
    presetMap = {
        { "Blackbody", getBlackBodyPalette(Interval(300, 12000), 6) },
        { "Galaxy", Palettes::GALAXY },
        { "Accretion", Palettes::ACCRETION },
        { "Stellar", Palettes::STELLAR },
    };
    for (auto& pair : PALETTE_ID_LIST) {
        presetMap.insert(pair.value(), Factory::getPalette(pair.key()));
    }

    wxArrayString items;
    for (const auto& e : presetMap) {
        items.Add(e.key().toUnicode());
    }
    presetBox->Set(items);
    presetBox->SetSelection(0);
    // this->update();
}

void PaletteSetup::loadPalettes(const Path& path) {
    fileMap.clear();
    for (Path file : FileSystem::iterateDirectory(path.parentPath())) {
        if (file.extension().string() == "csv") {
            Palette loaded = editor->getPalette();
            if (loaded.loadFromFile(path.parentPath() / file)) {
                presetMap.insert(file.string(), loaded.subsample(8));
            }
        }
    }
    wxArrayString items;
    int selectionIdx = 0, idx = 0;
    for (const auto& e : presetMap) {
        items.Add(e.key().toUnicode());
        if (e.key() == path.fileName().string()) {
            // this is the palette we selected
            selectionIdx = idx;
        }
        ++idx;
    }
    fileBox->Set(items);
    fileBox->SetSelection(selectionIdx);

    // this->update();
}

NAMESPACE_SPH_END
