#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Element.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/gauge.h>
#include <wx/sizer.h>

NAMESPACE_SPH_BEGIN

enum class ControlIds { QUANTITY_BOX };

MainWindow::MainWindow(Controller* parent, const GuiSettings& settings)
    : parent(parent) {
    // create the frame
    std::string title = settings.get<std::string>(GuiSettingsId::WINDOW_TITLE);
    wxSize size(
        settings.get<int>(GuiSettingsId::WINDOW_WIDTH), settings.get<int>(GuiSettingsId::WINDOW_HEIGHT));
    this->Create(nullptr, wxID_ANY, title.c_str(), wxDefaultPosition, size);

    // create all components
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* toolbar = new wxBoxSizer(wxHORIZONTAL);

    wxButton* button = new wxButton(this, wxID_ANY, "Start");
    button->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->start(); });
    toolbar->Add(button);

    button = new wxButton(this, wxID_ANY, "Pause");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->pause(); });

    button = new wxButton(this, wxID_ANY, "Stop");
    toolbar->Add(button);
    button->Bind(wxEVT_BUTTON, [parent](wxCommandEvent& UNUSED(evt)) { parent->stop(); });

    quantityBox = new wxComboBox(this, int(ControlIds::QUANTITY_BOX), "");
    quantityBox->SetWindowStyle(wxCB_SIMPLE | wxCB_READONLY);
    quantityBox->SetSelection(0);
    toolbar->Add(quantityBox);
    gauge = new wxGauge(this, wxID_ANY, 1000);
    gauge->SetValue(0);
    gauge->SetMinSize(wxSize(300, -1));
    toolbar->AddSpacer(200);
    toolbar->Add(gauge, 0, wxALIGN_CENTER_VERTICAL);

    sizer->Add(toolbar);

    pane = new OrthoPane(this, parent);
    sizer->Add(pane, 1, wxEXPAND);

    this->SetSizer(sizer);

    // connect event handlers
    Connect(wxEVT_COMBOBOX, wxCommandEventHandler(MainWindow::onComboBox));
    Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainWindow::onClose));
}

void MainWindow::setProgress(const float progress) {
    gauge->SetValue(int(progress * 1000.f));
}

void MainWindow::setElementList(ArrayView<std::string> elements) {
    wxArrayString items;
    for (std::string& s : elements) {
        items.Add(s.c_str());
    }
    quantityBox->Set(items);
}

void MainWindow::onClose(wxCloseEvent& evt) {
    // veto the event, we will close the window ourselves
    if (!parent->isQuitting()) {
        evt.Veto();
        parent->quit();
    }
    // don't wait till it's closed so that we don't block the main thread
}

void MainWindow::onComboBox(wxCommandEvent& evt) {
    evt.Skip();
    /*const int idx = quantityBox->GetSelection();
    Array<QuantityId> list = parent->getElementList();
    ASSERT(unsigned(idx) < unsigned(list.size()));
    std::unique_ptr<Abstract::Element> element = Factory::getElement(list[idx]);*/
    // pane->set evt.Skip();
}

NAMESPACE_SPH_END
