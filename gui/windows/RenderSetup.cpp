#include "gui/windows/RenderSetup.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include <wx/button.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

RenderSetup::RenderSetup(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Render setup") {
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    sizer->Add(new wxStaticText(this, wxID_ANY, "First data file"));
    wxFilePickerCtrl* dataFiles = new wxFilePickerCtrl(this,
        wxID_ANY,
        {},
        "Select first data file",
        wxFileSelectorDefaultWildcardStr,
        wxDefaultPosition,
        wxDefaultSize,
        wxFLP_OPEN | wxFLP_USE_TEXTCTRL | wxFLP_FILE_MUST_EXIST);
    dataFiles->SetToolTip(
        "Select the first file your simulation created. Simulation files can be set up in the 'Output' "
        "category of the simulation node. Use a 'data file' or a 'state file' for rendering.");
    sizer->Add(dataFiles, 0, wxEXPAND);

    sizer->Add(new wxStaticText(this, wxID_ANY, "Output directory"));
    wxDirPickerCtrl* outDir = new wxDirPickerCtrl(this,
        wxID_ANY,
        {},
        "Select output directory",
        wxDefaultPosition,
        wxDefaultSize,
        wxDIRP_USE_TEXTCTRL | wxDIRP_DIR_MUST_EXIST);
    sizer->Add(outDir, 0, wxEXPAND);

    wxBoxSizer* renderTypeSizer = new wxBoxSizer(wxHORIZONTAL);
    renderTypeSizer->Add(new wxStaticText(this, wxID_ANY, "Renderer"));
    renderTypeSizer->AddStretchSpacer();
    ComboBox* renderType = new ComboBox(this, "Renderer", 250);
    renderType->Append("Surface", (void*)RendererEnum::RAYMARCHER);
    renderType->Append("Volumetric", (void*)RendererEnum::VOLUME);
    renderType->Select(0);
    renderTypeSizer->Add(renderType);
    sizer->Add(renderTypeSizer, 0, wxEXPAND);

    wxBoxSizer* cameraTypeSizer = new wxBoxSizer(wxHORIZONTAL);
    cameraTypeSizer->Add(new wxStaticText(this, wxID_ANY, "Camera"));
    cameraTypeSizer->AddStretchSpacer();
    ComboBox* cameraType = new ComboBox(this, "Camera", 250);
    cameraType->Append("Perspective", (void*)CameraEnum::PERSPECTIVE);
    cameraType->Append("Orthographic", (void*)CameraEnum::ORTHO);
    cameraType->Append("Fisheye", (void*)CameraEnum::FISHEYE);
    cameraType->Select(0);
    cameraTypeSizer->Add(cameraType);
    sizer->Add(cameraTypeSizer, 0, wxEXPAND);

    wxRadioButton* fileOnly = new wxRadioButton(this, wxID_ANY, "Selected file only");
    wxRadioButton* sequence = new wxRadioButton(this, wxID_ANY, "File sequence");
    fileOnly->SetValue(true);
    wxBoxSizer* sequenceSizer = new wxBoxSizer(wxHORIZONTAL);
    sequenceSizer->Add(fileOnly);
    sequenceSizer->Add(sequence);
    sizer->Add(sequenceSizer);

    sizer->AddSpacer(20);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* createButton = new wxButton(this, wxID_ANY, "Create");

    auto doSetup = [=]() {
        wxString dataPath = dataFiles->GetPath();
        wxString outPath = outDir->GetPath();
        int cameraId = cameraType->GetSelection();
        int renderId = renderType->GetSelection();
        selectedCamera = CameraEnum(reinterpret_cast<int64_t>(cameraType->GetClientData(cameraId)));
        selectedRenderer = RendererEnum(reinterpret_cast<int64_t>(renderType->GetClientData(renderId)));
        firstFilePath = Path(String(dataPath.wc_str()));
        outputDir = Path(String(outPath.wc_str()));
        doSequence = sequence->GetValue();
        if (!FileSystem::pathExists(firstFilePath)) {
            wxMessageBox("Selected input file does not exist", "File not found");
            return false;
        }
        if (!FileSystem::pathExists(outputDir)) {
            wxMessageBox("Selected output directory does not exist", "Directory not found");
            return false;
        }
        return true;
    };

    createButton->Bind(wxEVT_BUTTON, [this, doSetup](wxCommandEvent& UNUSED(evt)) {
        if (doSetup()) {
            this->EndModal(wxID_OK);
        }
    });
    wxButton* renderButton = new wxButton(this, wxID_ANY, "Create && Render");
    renderButton->Bind(wxEVT_BUTTON, [this, doSetup](wxCommandEvent& UNUSED(evt)) {
        if (doSetup()) {
            doRender = true;
            this->EndModal(wxID_OK);
        }
    });
    wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_CANCEL); });
    buttonSizer->Add(createButton);
    buttonSizer->Add(renderButton);
    buttonSizer->Add(cancelButton);
    sizer->Add(buttonSizer, 0, wxALIGN_RIGHT);

    this->SetSizerAndFit(sizer);
}


NAMESPACE_SPH_END
