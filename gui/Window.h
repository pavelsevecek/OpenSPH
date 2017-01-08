#pragma once

#include "gui/Renderer.h"
#include "gui/Settings.h"
#include "objects/Object.h"
#include <wx/combobox.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds { BUTTON_START, QUANTITY_BOX };

class Window : public wxFrame {
private:
    Abstract::Renderer* renderer;
    wxComboBox* quantityBox;

public:
    Window(const std::shared_ptr<Storage>& storage, const GuiSettings& settings)
        : wxFrame(nullptr,
                  wxID_ANY,
                  settings.get<std::string>(GuiSettingsIds::WINDOW_TITLE).c_str(),
                  wxDefaultPosition,
                  wxSize(800, 600)) {
        wxBoxSizer* sizer   = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(new wxButton(this, int(ControlIds::BUTTON_START), "Start"));
        wxString quantities[] = { "Velocity", "Density", "Pressure", "Damage" };
        const int quantityCnt = storage->has(QuantityKey::DAMAGE) ? 4 : 3;
        quantityBox           = new wxComboBox(this,
                                     int(ControlIds::QUANTITY_BOX),
                                     "",
                                     wxDefaultPosition,
                                     wxDefaultSize,
                                     quantityCnt,
                                     quantities,
                                     wxCB_SIMPLE | wxCB_READONLY);
        this->Connect(wxEVT_COMBOBOX, wxCommandEventHandler(Window::onComboBox));
        quantityBox->SetSelection(0);
        toolbar->Add(quantityBox);
        //    this->Connect(wxEVT_BUTTON, wxCommandEventHandler(MyApp::OnButton), nullptr, window);
        sizer->Add(toolbar);

        switch (GUI_SETTINGS.get<RendererEnum>(GuiSettingsIds::RENDERER)) {
        case RendererEnum::OPENGL: {
            CustomGlPane* pane =
                new CustomGlPane(this, { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0 });
            sizer->Add(pane, 1, wxEXPAND);
            renderer = pane;
            break;
        }
        case RendererEnum::ORTHO:
            OrthoPane* pane = new OrthoPane(this, storage, settings);
            sizer->Add(pane, 1, wxEXPAND);
            renderer = pane;
            break;
        }

        this->SetSizer(sizer);
    }

    Abstract::Renderer* getRenderer() { return renderer; }

private:
    void onComboBox(wxCommandEvent& evt) {
        const int idx = quantityBox->GetSelection();
        switch (idx) {
        case 0:
            renderer->setQuantity(QuantityKey::POSITIONS);
            break;
        case 1:
            renderer->setQuantity(QuantityKey::DENSITY);
            break;
        case 2:
            renderer->setQuantity(QuantityKey::PRESSURE);
            break;
        case 3:
            renderer->setQuantity(QuantityKey::DAMAGE);
            break;
        }
        evt.Skip();
    }
};

NAMESPACE_SPH_END
