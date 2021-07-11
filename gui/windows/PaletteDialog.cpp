#include "gui/windows/PaletteDialog.h"
#include "gui/Factory.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/Spectrum.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/dcclient.h>
#include <wx/panel.h>
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

    virtual void drawText(const Coords p, const Flags<TextAlign>, const std::string& s) override {
        context->drawText(this->transform(p), TextAlign::VERTICAL_CENTER | TextAlign::HORIZONTAL_CENTER, s);
    }

    virtual void drawText(const Coords p, const Flags<TextAlign>, const std::wstring& s) override {
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
        FlippedRenderContext context(makeAuto<WxRenderContext>(dc));
        context.setFontSize(9);
        Rgba background(dc.GetBackground().GetColour());
        drawPalette(context, Pixel(40, 310), Pixel(40, 300), background.inverse(), palette);
    }
};

PaletteDialog::PaletteDialog(wxWindow* parent,
    wxSize size,
    const Palette initialPalette,
    Function<void(Palette)> setPalette)
    : wxFrame(parent, wxID_ANY, "Palette Dialog", wxGetMousePosition(), size)
    , setPaletteCallback(setPalette) {

    selected = initial = initialPalette;

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    canvas = new PaletteCanvas(this, initialPalette);
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

    const Size height = 30;
    rangeSizer->Add(new wxStaticText(this, wxID_ANY, "From: ", wxDefaultPosition, wxSize(-1, height)));
    FloatTextCtrl* lowerCtrl = new FloatTextCtrl(this, double(initialPalette.getInterval().lower()));
    rangeSizer->Add(lowerCtrl);

    rangeSizer->Add(new wxStaticText(this, wxID_ANY, "To: ", wxDefaultPosition, wxSize(-1, height)));
    FloatTextCtrl* upperCtrl = new FloatTextCtrl(this, double(initialPalette.getInterval().upper()));
    rangeSizer->Add(upperCtrl);

    upperCtrl->onValueChanged = [this](const Float value) {
        /// \todo deduplicate
        const Float lower = selected.getInterval().lower();
        Interval newRange(min(lower, value), max(lower, value));
        selected.setInterval(newRange);
        canvas->setPalette(selected);
        setPaletteCallback(selected);
    };
    lowerCtrl->onValueChanged = [this](const Float value) {
        const Float upper = selected.getInterval().upper();
        const Interval newRange(min(value, upper), max(value, upper));
        selected.setInterval(newRange);
        canvas->setPalette(selected);
        setPaletteCallback(selected);
    };

    mainSizer->Add(rangeSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(5);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* okButton = new wxButton(this, wxID_ANY, "OK");
    okButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        setPaletteCallback(selected);
        this->Close();
    });
    buttonSizer->Add(okButton);
    wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->Close(); });
    buttonSizer->Add(cancelButton);

    mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(10);

    this->SetSizerAndFit(mainSizer);

    // setup palette box
    paletteBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) { this->update(); });

    loadButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doOpenFileDialog("Load palette", { { "Palette files", "csv" } });
        if (!path) {
            return;
        }
        paletteMap.clear();
        for (Path file : FileSystem::iterateDirectory(path->parentPath())) {
            if (file.extension().native() == "csv") {
                Palette palette = initial;
                if (palette.loadFromFile(path->parentPath() / file)) {
                    paletteMap.insert(file.native(), palette);
                }
            }
        }
        wxArrayString items;
        int selectionIdx = 0, idx = 0;
        for (FlatMap<std::string, Palette>::Element& e : paletteMap) {
            items.Add(e.key);
            if (e.key == path->fileName().native()) {
                // this is the palette we selected
                selectionIdx = idx;
            }
            ++idx;
        }
        paletteBox->Set(items);
        paletteBox->SetSelection(selectionIdx);

        this->update();
    });
    resetButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->setDefaultPaletteList(); });

    this->setDefaultPaletteList();
}

static FlatMap<ExtColorizerId, std::string> PALETTE_ID_LIST = {
    { ColorizerId::VELOCITY, "Velocity" },
    { ColorizerId::TEMPERATURE, "Temperature" },
    { QuantityId::DEVIATORIC_STRESS, "Stress" },
    { QuantityId::DAMAGE, "Damage" },
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

void PaletteDialog::setDefaultPaletteList() {
    paletteMap = {
        { "(default)", initial },
        { "Blackbody", getBlackBodyPalette(Interval(300, 12000)) },
        { "Galaxy", Palettes::GALAXY },
        { "Accretion", Palettes::ACCRETION },
    };
    for (auto& pair : PALETTE_ID_LIST) {
        paletteMap.insert(pair.value, Factory::getPalette(pair.key));
    }

    wxArrayString items;
    for (FlatMap<std::string, Palette>::Element& e : paletteMap) {
        items.Add(e.key);
    }
    paletteBox->Set(items);
    paletteBox->SetSelection(0);
    this->update();
}

void PaletteDialog::update() {
    const int idx = paletteBox->GetSelection();
    const Interval range = selected.getInterval();
    selected = (paletteMap.begin() + idx)->value;
    selected.setInterval(range);
    canvas->setPalette(selected);
    setPaletteCallback(selected);
}

NAMESPACE_SPH_END
