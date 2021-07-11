#include "gui/windows/GuiSettingsDialog.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/Utils.h"
#include "gui/objects/Colorizer.h"
#include "gui/windows/Widgets.h"
#include "io/Path.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

const static FlatMap<PlotEnum, wxString> sPlotTypes = {
    { PlotEnum::TOTAL_MOMENTUM, "Total momentum" },
    { PlotEnum::TOTAL_ANGULAR_MOMENTUM, "Total angular momentum" },
    { PlotEnum::INTERNAL_ENERGY, "Total internal energy" },
    { PlotEnum::KINETIC_ENERGY, "Total kinetic energy" },
    { PlotEnum::TOTAL_ENERGY, "Total energy" },
    { PlotEnum::RELATIVE_ENERGY_CHANGE, "Relative change of total energy" },
    { PlotEnum::CURRENT_SFD, "Current SFD" },
    { PlotEnum::PREDICTED_SFD, "Predicted SFD" },
    { PlotEnum::SPEED_HISTOGRAM, "Speed histogram" },
    { PlotEnum::ANGULAR_HISTOGRAM_OF_VELOCITIES, "Angular histogram of velocities" },
    { PlotEnum::SELECTED_PARTICLE, "Selected particle" },
};

GuiSettingsDialog::GuiSettingsDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Visualization settings", wxDefaultPosition, wxSize(500, 340)) {
    const Project& project = Project::getInstance();
    const GuiSettings& gui = project.getGuiSettings();

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer* renderBox = new wxStaticBoxSizer(wxVERTICAL, this, "Rendering");

    wxBoxSizer* colorizerSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* colorizerText = new wxStaticText(renderBox->GetStaticBox(), wxID_ANY, "Default quantity");
    colorizerSizer->Add(colorizerText, 0, wxALIGN_CENTER_VERTICAL);
    colorizerSizer->AddStretchSpacer(1);
    colorizerBox = new ComboBox(renderBox->GetStaticBox(), "");
    ColorizerId defaultId = gui.get<ColorizerId>(GuiSettingsId::DEFAULT_COLORIZER);
    colorizerIds = getColorizerIds();
    for (const ExtColorizerId& id : colorizerIds) {
        SharedPtr<IColorizer> colorizer = Factory::getColorizer(project, id);
        colorizerBox->Append(colorizer->name());
        if (id == defaultId) {
            colorizerBox->SetSelection(colorizerBox->GetCount() - 1);
        }
    }
    colorizerSizer->Add(colorizerBox, 1, wxEXPAND);
    renderBox->Add(colorizerSizer);

    sizer->Add(renderBox, 1, wxEXPAND);


    wxStaticBoxSizer* plotBox = new wxStaticBoxSizer(wxVERTICAL, this, "Plots");
    wxGridSizer* plotGrid = new wxGridSizer(2, 1, 1);
    const Flags<PlotEnum> plotFlags = gui.getFlags<PlotEnum>(GuiSettingsId::PLOT_INTEGRALS);
    wxCheckBox* sfdCheck1 = nullptr;
    wxCheckBox* sfdCheck2 = nullptr;
    for (const auto& p : sPlotTypes) {
        wxCheckBox* check = new wxCheckBox(plotBox->GetStaticBox(), wxID_ANY, p.value);
        check->SetValue(plotFlags.has(p.key));
        if (p.key == PlotEnum::CURRENT_SFD) {
            sfdCheck1 = check;
        } else if (p.key == PlotEnum::PREDICTED_SFD) {
            sfdCheck2 = check;
        }
        plotGrid->Add(check);
        plotBoxMap.insert(check, p.key);
    }
    plotBox->Add(plotGrid);
    sizer->Add(plotBox, 1, wxEXPAND);

    wxBoxSizer* periodSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* periodText = new wxStaticText(plotBox->GetStaticBox(), wxID_ANY, "Initial period [s]");
    periodSizer->Add(periodText, 0, wxALIGN_CENTER_VERTICAL);
    const Float period = gui.get<Float>(GuiSettingsId::PLOT_INITIAL_PERIOD);
    periodCtrl = new FloatTextCtrl(plotBox->GetStaticBox(), period, Interval(0._f, LARGE));
    periodSizer->AddStretchSpacer(1);
    periodSizer->Add(periodCtrl, 0, wxALIGN_CENTER_VERTICAL);
    plotBox->Add(periodSizer, 1, wxEXPAND);

    wxBoxSizer* overplotSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* overplotText = new wxStaticText(plotBox->GetStaticBox(), wxID_ANY, "Reference SFD");
    overplotSizer->Add(overplotText, 0, wxALIGN_CENTER_VERTICAL);
    const std::string overplotSfd = gui.get<std::string>(GuiSettingsId::PLOT_OVERPLOT_SFD);
    overplotPath = new wxTextCtrl(plotBox->GetStaticBox(), wxID_ANY, overplotSfd);
    overplotPath->Enable(plotFlags.hasAny(PlotEnum::CURRENT_SFD, PlotEnum::PREDICTED_SFD));
    overplotPath->SetMinSize(wxSize(250, -1));
    overplotSizer->AddStretchSpacer(1);
    overplotSizer->Add(overplotPath, 0, wxALIGN_CENTER_VERTICAL);
    wxButton* overplotBrowse = new wxButton(plotBox->GetStaticBox(), wxID_ANY, "Select...");
    overplotBrowse->Enable(plotFlags.hasAny(PlotEnum::CURRENT_SFD, PlotEnum::PREDICTED_SFD));
    overplotSizer->Add(overplotBrowse, 0, wxALIGN_CENTER_VERTICAL);
    plotBox->Add(overplotSizer, 1, wxEXPAND);

    sizer->AddSpacer(8);
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* okButton = new wxButton(this, wxID_ANY, "OK");
    wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    buttonSizer->AddStretchSpacer(1);
    buttonSizer->Add(okButton);
    buttonSizer->Add(cancelButton);
    sizer->Add(buttonSizer, 1, wxEXPAND);

    this->SetSizer(sizer);
    this->Layout();

    auto enableOverplot = [=](wxCommandEvent& UNUSED(evt)) {
        const bool doEnable = sfdCheck1->IsChecked() || sfdCheck2->IsChecked();
        overplotPath->Enable(doEnable);
        overplotBrowse->Enable(doEnable);
    };
    sfdCheck1->Bind(wxEVT_CHECKBOX, enableOverplot);
    sfdCheck2->Bind(wxEVT_CHECKBOX, enableOverplot);

    overplotBrowse->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doOpenFileDialog("Select reference SFD", {});
        if (path) {
            overplotPath->SetValue(path->native());
        }
    });

    okButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->commit(); });
    cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_CANCEL); });
}

void GuiSettingsDialog::commit() {
    Project& project = Project::getInstance();
    GuiSettings& gui = project.getGuiSettings();

    const int index = colorizerBox->GetSelection();
    SPH_ASSERT(index != wxNOT_FOUND);
    const ColorizerId id(colorizerIds[index]);
    gui.set(GuiSettingsId::DEFAULT_COLORIZER, id);

    Flags<PlotEnum> enabledPlots = EMPTY_FLAGS;
    for (const auto& p : plotBoxMap) {
        enabledPlots.setIf(p.value, p.key->GetValue());
    }
    gui.set(GuiSettingsId::PLOT_INTEGRALS, enabledPlots);

    gui.set(GuiSettingsId::PLOT_INITIAL_PERIOD, Float(periodCtrl->getValue()));
    gui.set(GuiSettingsId::PLOT_OVERPLOT_SFD, std::string(overplotPath->GetValue()));

    this->EndModal(wxID_OK);
}

GuiSettingsDialog::~GuiSettingsDialog() = default;

NAMESPACE_SPH_END
