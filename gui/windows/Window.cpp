#include "gui/windows/Window.h"
#include "gui/MainLoop.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "system/LogFile.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds { BUTTON_START, BUTTON_STOP, QUANTITY_BOX };

Window::Window(const std::shared_ptr<Storage>& storage,
    const GuiSettings& settings,
    const std::function<void(void)>& onRestart)
    : wxFrame(nullptr,
          wxID_ANY,
          settings.get<std::string>(GuiSettingsIds::WINDOW_TITLE).c_str(),
          wxDefaultPosition,
          wxSize(800, 600))
    , storage(storage)
    , onRestart(onRestart) {
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* toolbar = new wxBoxSizer(wxHORIZONTAL);
    toolbar->Add(new wxButton(this, int(ControlIds::BUTTON_START), "Start"));
    toolbar->Add(new wxButton(this, int(ControlIds::BUTTON_STOP), "Stop"));
    this->Connect(wxEVT_BUTTON, wxCommandEventHandler(Window::onButton));
    wxString quantities[] = { "Velocity", "Density", "Pressure", "Energy", "Stress", "Damage" };
    const int quantityCnt = storage->has(QuantityIds::DAMAGE) ? 6 : 5;
    quantityBox = new wxComboBox(this,
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
    gauge = new wxGauge(this, wxID_ANY, 1000);
    gauge->SetValue(0);
    gauge->SetMinSize(wxSize(300, -1));
    toolbar->AddSpacer(200);
    toolbar->Add(gauge, 0, wxALIGN_CENTER_VERTICAL);

    sizer->Add(toolbar);

    switch (GuiSettings::getDefaults().get<RendererEnum>(GuiSettingsIds::RENDERER)) {
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

Abstract::Renderer* Window::getRenderer() {
    return renderer;
}

void Window::setProgress(const float progress) {
    gauge->SetValue(int(progress * 1000.f));
}

void Window::onButton(wxCommandEvent& evt) {
    switch (evt.GetId()) {
    case int(ControlIds::BUTTON_START):
        abortRun = false;
        onRestart();
        break;
    case int(ControlIds::BUTTON_STOP):
        abortRun = true;
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void Window::onComboBox(wxCommandEvent& evt) {
    const int idx = quantityBox->GetSelection();
    switch (idx) {
    case 0:
        renderer->setQuantity(QuantityIds::POSITIONS);
        break;
    case 1:
        renderer->setQuantity(QuantityIds::DENSITY);
        break;
    case 2:
        renderer->setQuantity(QuantityIds::PRESSURE);
        break;
    case 3:
        renderer->setQuantity(QuantityIds::ENERGY);
        break;
    case 4:
        renderer->setQuantity(QuantityIds::DEVIATORIC_STRESS);
        break;
    case 5:
        renderer->setQuantity(QuantityIds::DAMAGE);
        break;
    }
    renderer->draw(storage);
    evt.Skip();
}

NAMESPACE_SPH_END
