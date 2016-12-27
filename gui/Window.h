#pragma once

#include "gui/Renderer.h"
#include "objects/Object.h"
#include <wx/combobox.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds { BUTTON_START, QUANTITY_BOX };

class Window : public wxFrame {
private:
    Abstract::Renderer* renderer;

public:
    Window()
        : wxFrame(nullptr, wxID_ANY, "SPH", wxDefaultPosition, wxSize(800, 600)) {
        wxBoxSizer* sizer   = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(new wxButton(this, int(ControlIds::BUTTON_START), "Start"));
        wxString quantities[]   = { "Velocity", "Damage", "Density", "Pressure" };
        wxComboBox* quantityBox = new wxComboBox(this,
                                                 int(ControlIds::QUANTITY_BOX),
                                                 "",
                                                 wxDefaultPosition,
                                                 wxDefaultSize,
                                                 4,
                                                 quantities,
                                                 wxCB_SIMPLE | wxCB_READONLY);
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
            OrthoPane* pane = new OrthoPane(this);
            sizer->Add(pane, 1, wxEXPAND);
            renderer = pane;
            break;
        }

        this->SetSizer(sizer);
    }

    Abstract::Renderer* getRenderer() { return renderer; }
};

NAMESPACE_SPH_END
