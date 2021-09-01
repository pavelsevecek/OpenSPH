#include "gui/windows/PaletteDialog.h"
#include "gui/Factory.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/Spectrum.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include "post/Plot.h"
#include "post/Point.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

void drawPalette(wxDC& dc, const wxPoint origin, const wxSize size, const Palette& palette) {
    // draw palette
    wxPen pen = dc.GetPen();
    for (int i = 0; i < size.x; ++i) {
        const float x = float(i) / float(size.x);
        const Rgba color = palette(x);
        pen.SetColour(wxColour(color));
        dc.SetPen(pen);
        dc.DrawLine(wxPoint(origin.x + i, origin.y), wxPoint(origin.x + i, origin.y + size.y));
    }
}

void drawTics(wxDC& dc, const wxPoint origin, const wxSize size, const Rgba& lineColor, const ColorLut& lut) {
    const Interval interval = lut.getInterval();
    const PaletteScale scale = lut.getScale();

    Array<Float> tics;
    switch (scale) {
    case PaletteScale::LINEAR:
        tics = getLinearTics(interval, 4);
        break;
    case PaletteScale::LOGARITHMIC:
        tics = getLogTics(interval, 4);
        break;
    case PaletteScale::HYBRID: {
        const Size ticsCnt = 5;
        // tics currently not implemented, so just split the range to equidistant steps
        for (Size i = 0; i < ticsCnt; ++i) {
            tics.push(lut.relativeToPalette(double(i) / (ticsCnt - 1)));
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
    wxPen pen = dc.GetPen();
    pen.SetColour(wxColour(lineColor));
    dc.SetPen(pen);

    for (Float tic : tics) {
        const double value = lut.paletteToRelative(tic);
        const int i = int(value * size.x);
        dc.DrawLine(wxPoint(origin.x + i, origin.y), wxPoint(origin.x + i, origin.y + 6));
        dc.DrawLine(wxPoint(origin.x + i, origin.y + size.y - 6), wxPoint(origin.x + i, origin.y + size.y));

        String text = toPrintableString(tic, 1, 1000);
        wxSize ext = dc.GetTextExtent(text.toUnicode());
        dc.DrawText(text.toUnicode(), wxPoint(origin.x + i, origin.y - 10) - ext / 2);
    }
}

class PaletteViewCanvas : public wxPanel {
protected:
    ColorLut lut;

public:
    PaletteViewCanvas(wxWindow* parent, const ColorLut lut)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
        , lut(lut) {
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteViewCanvas::onPaint));
        this->SetMinSize(wxSize(300, 100));
    }

    void setLut(const ColorLut& newLut) {
        lut = newLut;
        this->Refresh();
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        wxFont font = wxSystemSettings::GetFont(wxSystemFont::wxSYS_DEFAULT_GUI_FONT);
        font.SetPointSize(9);
        SPH_ASSERT(font.IsOk());
        dc.SetFont(font);
        const Rgba background(dc.GetBackground().GetColour());
        const Rgba lineColor = background.inverse();
        const wxPoint origin(20, 30);
        const wxSize size = wxSize(this->GetMinSize().x - 2 * origin.x, 40);
        drawPalette(dc, origin, size, lut.getPalette());
        drawTics(dc, origin, size, lineColor, lut);
    }
};

ColorLutPanel::ColorLutPanel(wxWindow* parent, wxSize size, const ColorLut lut)
    : wxPanel(parent, wxID_ANY)
    , initial(lut)
    , selected(lut) {

    this->SetMinSize(size);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    canvas = new PaletteViewCanvas(this, initial);
    mainSizer->Add(canvas, 0, wxALIGN_CENTER_HORIZONTAL);

    wxBoxSizer* selectionSizer = new wxBoxSizer(wxHORIZONTAL);
    paletteBox = new ComboBox(this, "Select palette ...");
    selectionSizer->Add(paletteBox);

    wxButton* loadButton = new wxButton(this, wxID_ANY, "Load");
    selectionSizer->Add(loadButton);

    wxButton* resetButton = new wxButton(this, wxID_ANY, "Reset");
    selectionSizer->Add(resetButton);

    mainSizer->Add(selectionSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(5);

    wxBoxSizer* rangeSizer = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText* text = new wxStaticText(this, wxID_ANY, "From ");
    rangeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    lowerCtrl = new FloatTextCtrl(this, double(initial.getInterval().lower()));
    rangeSizer->Add(lowerCtrl);
    rangeSizer->AddSpacer(30);

    text = new wxStaticText(this, wxID_ANY, "To ");
    rangeSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    upperCtrl = new FloatTextCtrl(this, double(initial.getInterval().upper()));
    rangeSizer->Add(upperCtrl);

    upperCtrl->onValueChanged = [this](const Float value) {
        /// \todo deduplicate
        const Float lower = selected.getInterval().lower();
        if (lower >= value) {
            return false;
        }
        selected.setInterval(Interval(lower, value));
        canvas->setLut(selected);
        onLutChanged.callIfNotNull(selected);
        return true;
    };
    lowerCtrl->onValueChanged = [this](const Float value) {
        const Float upper = selected.getInterval().upper();
        if (value >= upper) {
            return false;
        }
        if (selected.getScale() == PaletteScale::LOGARITHMIC && value <= 0.f) {
            return false;
        }
        selected.setInterval(Interval(value, upper));
        canvas->setLut(selected);
        onLutChanged.callIfNotNull(selected);
        return true;
    };

    mainSizer->Add(rangeSizer, 0, wxALIGN_CENTER_HORIZONTAL);

    this->SetSizerAndFit(mainSizer);

    // setup palette box
    paletteBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) { this->update(); });

    loadButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doOpenFileDialog("Load palette", { { "Palette files", "csv" } });
        if (!path) {
            return;
        }
        this->loadPalettes(path.value());
    });
    resetButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->setDefaultPaletteList(); });

    this->setDefaultPaletteList();
}

void ColorLutPanel::setLut(const ColorLut& lut) {
    selected = initial = lut;
    paletteMap.insert("Current", 0, initial.getPalette());
    paletteBox->SetSelection(0);
    canvas->setLut(selected);
    lowerCtrl->setValue(initial.getInterval().lower());
    upperCtrl->setValue(initial.getInterval().upper());
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

const Palette GALAXY({
    { 0.f, Rgba(0.f, 0.02f, 0.09f) },
    { 0.25f, Rgba(0.4f, 0.106f, 0.38f) },
    { 0.5f, Rgba(0.78f, 0.18f, 0.38f) },
    { 0.75f, Rgba(0.91f, 0.56f, 0.81f) },
    { 1.f, Rgba(0.29f, 0.69f, 0.93f) },
});

const Palette ACCRETION({
    { 0.f, Rgba(0.43f, 0.70f, 1.f) },
    { 0.2f, Rgba(0.5f, 0.5f, 0.5f) },
    { 0.4f, Rgba(0.65f, 0.12f, 0.01f) },
    { 0.6f, Rgba(0.79f, 0.38f, 0.02f) },
    { 0.8f, Rgba(0.93f, 0.83f, 0.34f) },
    { 1.f, Rgba(0.94f, 0.90f, 0.84f) },
});

const Palette STELLAR({
    { 0.f, Rgba(1.f, 0.75f, 0.1f) },
    { 0.33f, Rgba(0.75f, 0.25f, 0.1f) },
    { 0.66f, Rgba(0.4f, 0.7f, 1.f) },
    { 1.f, Rgba(0.2f, 0.4f, 0.8f) },
});

} // namespace Palettes

void ColorLutPanel::setDefaultPaletteList() {
    paletteMap = {
        { "Current", initial.getPalette() },
        { "Blackbody", getBlackBodyPalette(Interval(300, 12000)) },
        { "Galaxy", Palettes::GALAXY },
        { "Accretion", Palettes::ACCRETION },
        { "Stellar", Palettes::STELLAR },
    };
    for (auto& pair : PALETTE_ID_LIST) {
        paletteMap.insert(pair.value(), Factory::getPalette(pair.key()));
    }

    wxArrayString items;
    for (const auto& e : paletteMap) {
        items.Add(e.key().toUnicode());
    }
    paletteBox->Set(items);
    paletteBox->SetSelection(0);
    this->update();
}

void ColorLutPanel::loadPalettes(const Path& path) {
    paletteMap.clear();
    for (Path file : FileSystem::iterateDirectory(path.parentPath())) {
        if (file.extension().string() == "csv") {
            Palette palette = initial.getPalette();
            if (palette.loadCsvFromFile(path.parentPath() / file)) {
                paletteMap.insert(file.string(), palette);
            }
        }
    }
    wxArrayString items;
    int selectionIdx = 0, idx = 0;
    for (const auto& e : paletteMap) {
        items.Add(e.key().toUnicode());
        if (e.key() == path.fileName().string()) {
            // this is the palette we selected
            selectionIdx = idx;
        }
        ++idx;
    }
    paletteBox->Set(items);
    paletteBox->SetSelection(selectionIdx);

    this->update();
}

void ColorLutPanel::update() {
    const int idx = paletteBox->GetSelection();
    const Interval range = selected.getInterval();
    selected.setPalette((paletteMap.begin() + idx)->value());
    selected.setInterval(range);
    canvas->setLut(selected);
    onLutChanged.callIfNotNull(selected);
}

NAMESPACE_SPH_END
