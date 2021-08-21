#include "gui/windows/PaletteDialog.h"
#include "gui/Factory.h"
#include "gui/objects/Colorizer.h"
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

class FlippedRenderContext : public IRenderContext {
private:
    AutoPtr<IRenderContext> context;

public:
    explicit FlippedRenderContext(AutoPtr<IRenderContext>&& context)
        : context(std::move(context)) {}

    virtual Pixel size() const override {
        const Pixel p = context->size();
        return Pixel(p.y, p.x);
    }

    virtual void setColor(const Rgba& color, const Flags<ColorFlag> flags) override {
        context->setColor(color, flags);
    }

    virtual void setThickness(const float thickness) override {
        context->setThickness(thickness);
    }

    virtual void setFontSize(const int fontSize) override {
        context->setFontSize(fontSize);
    }

    virtual void fill(const Rgba& color) override {
        context->fill(color);
    }

    virtual void drawLine(const Coords p1, const Coords p2) override {
        context->drawLine(this->transform(p1), this->transform(p2));
    }

    virtual void drawCircle(const Coords center, const float radius) override {
        context->drawCircle(this->transform(center), radius);
    }

    virtual void drawTriangle(const Coords, const Coords, const Coords) override {
        NOT_IMPLEMENTED;
    }

    virtual void drawBitmap(const Coords, const Bitmap<Rgba>&) override {
        NOT_IMPLEMENTED;
    }

    virtual void drawText(const Coords p, const Flags<TextAlign>, const String& s) override {
        context->drawText(this->transform(p), TextAlign::VERTICAL_CENTER | TextAlign::HORIZONTAL_CENTER, s);
    }

private:
    Pixel transform(const Pixel& p) {
        return Pixel(context->size().x - p.y, p.x);
    }

    Coords transform(const Coords& p) {
        return Coords(context->size().x - p.y, p.x);
    }
};

void drawPalette(IRenderContext& context,
    const Pixel origin,
    const Pixel size,
    const Rgba& lineColor,
    const Palette& palette) {

    // draw palette
    for (int i = 0; i < size.y; ++i) {
        const float value = palette.relativeToPalette(float(i) / (size.y - 1));
        context.setColor(palette(value), ColorFlag::LINE);
        context.drawLine(Coords(origin.x, origin.y - i), Coords(origin.x + size.x, origin.y - i));
    }

    // draw tics
    const Interval interval = palette.getInterval();
    const PaletteScale scale = palette.getScale();

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
            tics.push(palette.relativeToPalette(float(i) / (ticsCnt - 1)));
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
    context.setColor(lineColor, ColorFlag::LINE | ColorFlag::TEXT);
    for (Float tic : tics) {
        const float value = palette.paletteToRelative(float(tic));
        const int i = int(value * size.y);
        context.drawLine(Coords(origin.x, origin.y - i), Coords(origin.x + 6, origin.y - i));
        context.drawLine(
            Coords(origin.x + size.x - 6, origin.y - i), Coords(origin.x + size.x, origin.y - i));

        String text = toPrintableString(tic, 1, 1000);
        context.drawText(
            Coords(origin.x - 15, origin.y - i), TextAlign::LEFT | TextAlign::VERTICAL_CENTER, text);
    }
}

class PaletteCanvas : public wxPanel {
private:
    Palette palette;

public:
    PaletteCanvas(wxWindow* parent, const Palette palette)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
        , palette(palette) {
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteCanvas::onPaint));
        this->SetMinSize(wxSize(320, 100));
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
        FlippedRenderContext context(makeAuto<WxRenderContext>(dc));
        context.setFontSize(9);
        Rgba background(dc.GetBackground().GetColour());
        drawPalette(context, Pixel(40, 310), Pixel(40, 300), background.inverse(), palette);
    }
};

PalettePanel::PalettePanel(wxWindow* parent, wxSize size, const Palette palette)
    : wxPanel(parent, wxID_ANY)
    , initial(palette)
    , selected(palette) {

    this->SetMinSize(size);

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    canvas = new PaletteCanvas(this, initial);
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
        canvas->setPalette(selected);
        onPaletteChanged.callIfNotNull(selected);
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
        canvas->setPalette(selected);
        onPaletteChanged.callIfNotNull(selected);
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

void PalettePanel::setPalette(const Palette& palette) {
    selected = initial = palette;
    paletteMap.insert("Current", 0, initial);
    paletteBox->SetSelection(0);
    canvas->setPalette(selected);
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

const Palette GALAXY({ { 0.001f, Rgba(0.f, 0.02f, 0.09f) },
                         { 0.01f, Rgba(0.4f, 0.106f, 0.38f) },
                         { 0.1f, Rgba(0.78f, 0.18f, 0.38f) },
                         { 1.f, Rgba(0.91f, 0.56f, 0.81f) },
                         { 10.f, Rgba(0.29f, 0.69f, 0.93f) } },
    PaletteScale::LOGARITHMIC);

const Palette ACCRETION({ { 0.001f, Rgba(0.43f, 0.70f, 1.f) },
                            { 0.01f, Rgba(0.5f, 0.5f, 0.5f) },
                            { 0.1f, Rgba(0.65f, 0.12f, 0.01f) },
                            { 1.f, Rgba(0.79f, 0.38f, 0.02f) },
                            { 10.f, Rgba(0.93f, 0.83f, 0.34f) },
                            { 100.f, Rgba(0.94f, 0.90f, 0.84f) } },
    PaletteScale::LOGARITHMIC);

} // namespace Palettes

void PalettePanel::setDefaultPaletteList() {
    paletteMap = {
        { "Current", initial },
        { "Blackbody", getBlackBodyPalette(Interval(300, 12000)) },
        { "Galaxy", Palettes::GALAXY },
        { "Accretion", Palettes::ACCRETION },
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

void PalettePanel::loadPalettes(const Path& path) {
    paletteMap.clear();
    for (Path file : FileSystem::iterateDirectory(path.parentPath())) {
        if (file.extension().string() == "csv") {
            Palette palette = initial;
            if (palette.loadFromFile(path.parentPath() / file)) {
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

void PalettePanel::update() {
    const int idx = paletteBox->GetSelection();
    const Interval range = selected.getInterval();
    selected = (paletteMap.begin() + idx)->value();
    selected.setInterval(range);
    canvas->setPalette(selected);
    onPaletteChanged.callIfNotNull(selected);
}

NAMESPACE_SPH_END
