#include "gui/windows/PaletteDialog.h"
#include "gui/Factory.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/ParticleRenderer.h"
#include "io/FileSystem.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/dcclient.h>
#include <wx/filedlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
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
        drawPalette(context, Pixel(40, 310), Pixel(40, 300), Rgba::white(), palette);
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
    paletteBox = new wxComboBox(this, wxID_ANY, "Select palette ...");
    paletteBox->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
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
    wxSpinCtrlDouble* lowerSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, height));
    const int range = 1000 * initialPalette.getInterval().size();
    lowerSpinner->SetRange(-range, range);
    const int lowerDigits = max(0, 2 - int(log10(abs(initialPalette.getInterval().lower()) + EPS)));
    lowerSpinner->SetDigits(lowerDigits);
    lowerSpinner->SetValue(initialPalette.getInterval().lower());
    lowerSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this, parent, lowerSpinner](wxSpinDoubleEvent& UNUSED(evt)) {
        const double value = lowerSpinner->GetValue();
        const Interval newRange(value, selected.getInterval().upper());
        selected.setInterval(newRange);
        canvas->setPalette(selected);
        setPaletteCallback(selected);
    });
    rangeSizer->Add(lowerSpinner);

    rangeSizer->Add(new wxStaticText(this, wxID_ANY, "To: ", wxDefaultPosition, wxSize(-1, height)));
    wxSpinCtrlDouble* upperSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, height));
    upperSpinner->SetRange(-range, range);
    const int upperDigits = max(0, 2 - int(log10(abs(initialPalette.getInterval().upper()) + EPS)));
    upperSpinner->SetDigits(upperDigits);
    upperSpinner->SetValue(initialPalette.getInterval().upper());
    upperSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this, parent, upperSpinner](wxSpinDoubleEvent& UNUSED(evt)) {
        /// \todo deduplicate
        const double value = upperSpinner->GetValue();
        const Interval newRange(selected.getInterval().lower(), value);
        selected.setInterval(newRange);
        canvas->setPalette(selected);
        setPaletteCallback(selected);
    });
    rangeSizer->Add(upperSpinner);

    mainSizer->Add(rangeSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    mainSizer->AddSpacer(5);


    /*wxBoxSizer* transformSizer = wxBoxSizer(wxHORIZONTAL);
    wxSpinCtrlDouble* brightnessSpinner =
        new wxSpinCtrlDouble(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, height));
    brightnessSpinner->SetDigits(2);
    brightnessSpinner->SetRange(0.01, 100.);
    brightnessSpinner->Bind(wxEVT_SPINCTRLDOUBLE, [this, brightnessSpinner](wxSpinDoubleEvent& UNUSED(evt)) {
        const double value = brightnessSpinner->GetValue();
            const Interval newRange(value, selected.getInterval().upper());
            selected.setInterval(newRange);
            canvas->setPalette(selected);
            setPaletteCallback(selected);
    });
    transformSizer->Add(brightnessSpinner);
    mainSizer->Add(transformSizer);*/

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
        static std::string desc = "Palette files (*.csv)|*.csv";
        static std::string defaultDir;
        wxFileDialog dialog(nullptr, "Load palette", defaultDir, "", desc, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() == wxID_CANCEL) {
            return;
        }
        const Path path(std::string(dialog.GetPath()));
        defaultDir = path.parentPath().native();

        paletteMap.clear();
        for (Path file : FileSystem::iterateDirectory(path.parentPath())) {
            if (file.extension().native() == "csv") {
                Palette palette = initial;
                if (palette.loadFromFile(path.parentPath() / file)) {
                    paletteMap.insert(file.native(), palette);
                }
            }
        }
        wxArrayString items;
        int selectionIdx = 0, idx = 0;
        for (FlatMap<std::string, Palette>::Element& e : paletteMap) {
            items.Add(e.key);
            if (e.key == path.fileName().native()) {
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

void PaletteDialog::setDefaultPaletteList() {
    paletteMap = {
        { "Default", initial },
        { "Grayscale", Factory::getPalette(ColorizerId(QuantityId::DAMAGE)) },
        { "Hot and cold", Factory::getPalette(ColorizerId::DENSITY_PERTURBATION) },
        { "Velocity", Factory::getPalette(ColorizerId::VELOCITY) },
    };

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