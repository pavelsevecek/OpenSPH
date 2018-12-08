#pragma once

#include "gui/objects/Colorizer.h"
#include "gui/objects/Palette.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/ParticleRenderer.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/dcclient.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>

NAMESPACE_SPH_BEGIN

class PaletteDialog : public wxFrame {
private:
    class Canvas : public wxPanel {
    private:
        Palette palette;

    public:
        Canvas(wxWindow* parent, const Palette palette)
            : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
            , palette(palette) {
            this->Connect(wxEVT_PAINT, wxPaintEventHandler(Canvas::onPaint));
            this->SetMinSize(wxSize(100, 240));
        }

        void setPalette(const Palette newPalette) {
            palette = newPalette;
            this->Refresh();
        }

    private:
        void onPaint(wxPaintEvent& UNUSED(evt)) {
            wxPaintDC dc(this);
            WxRenderContext context(dc);
            context.setFontSize(9);
            drawPalette(context, Pixel(40, 201), Rgba::white(), palette);
        }
    };

    struct PaletteDesc {
        std::string name;
        ColorizerId id;
    };

    Array<PaletteDesc> paletteDescs;

    Palette selected;

public:
    PaletteDialog(wxWindow* parent,
        wxSize size,
        const Palette initialPalette,
        Function<void(Palette)> setPalette)
        : wxFrame(parent, wxID_ANY, "Palette Dialog", wxGetMousePosition(), size) {

        wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

        wxBoxSizer* controlsSizer = new wxBoxSizer(wxVERTICAL);

        wxSpinCtrl* lowerSpinner = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1));
        lowerSpinner->SetRange(-1e8, 1e8);
        lowerSpinner->SetValue(initialPalette.getInterval().lower());
        lowerSpinner->Bind(wxEVT_SPINCTRL, [parent](wxSpinEvent& UNUSED(evt)) {});
        controlsSizer->Add(lowerSpinner);

        wxSpinCtrl* upperSpinner = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1));
        upperSpinner->SetRange(-1e8, 1e8);
        upperSpinner->SetValue(initialPalette.getInterval().upper());
        upperSpinner->Bind(wxEVT_SPINCTRL, [parent](wxSpinEvent& UNUSED(evt)) {});
        controlsSizer->Add(upperSpinner);

        wxComboBox* box = new wxComboBox(this, wxID_ANY, "Palette type");
        box->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
        box->SetSelection(0);
        controlsSizer->Add(box);

        wxButton* okButton = new wxButton(this, wxID_ANY, "OK");
        okButton->Bind(wxEVT_BUTTON, [this, setPalette](wxCommandEvent& UNUSED(evt)) {
            setPalette(selected);
            this->Close();
        });
        controlsSizer->Add(okButton);
        wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
        cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->Close(); });
        controlsSizer->Add(cancelButton);

        mainSizer->Add(controlsSizer);

        Canvas* canvas = new Canvas(this, initialPalette);
        mainSizer->Add(canvas);

        this->SetSizer(mainSizer);

        // setup palette box
        box->Bind(wxEVT_COMBOBOX, [this, box, canvas, initialPalette](wxCommandEvent& UNUSED(evt)) {
            const int idx = box->GetSelection();
            ColorizerId id = paletteDescs[idx].id;
            selected = Factory::getPalette(id, initialPalette.getInterval());
            canvas->setPalette(selected);
        });

        paletteDescs = {
            { "Grayscale", ColorizerId(QuantityId::DAMAGE) },
            { "Cold and hot", ColorizerId::DENSITY_PERTURBATION },
            { "Velocity", ColorizerId::VELOCITY },
        };

        wxArrayString items;
        for (PaletteDesc& desc : paletteDescs) {
            items.Add(desc.name.c_str());
        }
        box->Set(items);
    }

    Optional<Palette> getSelectedPalette() const {
        return selected;
    }
};

NAMESPACE_SPH_END
