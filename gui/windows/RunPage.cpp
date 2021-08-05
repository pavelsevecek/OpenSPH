#include "gui/windows/RunPage.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayMarcher.h"
#include "gui/renderers/VolumeRenderer.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/PaletteDialog.h"
#include "gui/windows/ParticleProbe.h"
#include "gui/windows/PlotView.h"
#include "gui/windows/ProgressPanel.h"
#include "gui/windows/TimeLine.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "sph/Diagnostics.h"
#include "thread/CheckFunction.h"
#include "thread/Pool.h"
#include "timestepping/TimeStepCriterion.h"
#include <fstream>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/gauge.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>
// needs to be included after framemanager
#include <wx/aui/dockart.h>

NAMESPACE_SPH_BEGIN

class TimeLineCallbacks : public ITimeLineCallbacks {
private:
    Controller* parent;

public:
    explicit TimeLineCallbacks(Controller* parent)
        : parent(parent) {}

    virtual void frameChanged(const Path& newFile) const override {
        parent->open(newFile, false);
    }

    virtual void startSequence(const Path& firstFile) const override {
        parent->open(firstFile, true);
    }

    virtual void stop() const override {
        parent->stop(false);
    }

    virtual void pause() const override {
        parent->pause();
    }
};

RunPage::RunPage(wxWindow* window, Controller* parent, GuiSettings& settings)
    : ClosablePage(window, "simulation")
    , controller(parent)
    , gui(settings) {
    manager = makeAuto<wxAuiManager>(this);

    wxPanel* visBar = createVisBar();
    pane = alignedNew<OrthoPane>(this, parent, settings);

    timelineBar = alignedNew<TimeLine>(this, Path(), makeShared<TimeLineCallbacks>(parent));
    progressBar = alignedNew<ProgressPanel>(this);

    wxAuiPaneInfo info;

    // info.Top().MinSize(wxSize(-1, 35)).CaptionVisible(false).DockFixed(true).CloseButton(false);
    // manager->AddPane(toolBar, info);

    info.Center().MinSize(wxSize(300, 300)).CaptionVisible(false).DockFixed(true).CloseButton(false);
    manager->AddPane(&*pane, info);

    info.Bottom().MinSize(wxSize(-1, 40)).CaptionVisible(false).DockFixed(true).CloseButton(false);
    manager->AddPane(timelineBar, info.Show(false));
    manager->AddPane(progressBar, info.Show(true));

    Flags<PaneEnum> paneIds = settings.getFlags<PaneEnum>(GuiSettingsId::DEFAULT_PANES);
    const Optional<Palette> palette = controller->getCurrentColorizer()->getPalette();
    if (paneIds.has(PaneEnum::PALETTE) && palette) {
        palettePanel = alignedNew<PalettePanel>(this, wxSize(300, -1), palette.value());
        palettePanel->onPaletteChanged = [this](const Palette& palette) {
            controller->setPaletteOverride(palette);
        };
        info.Left()
            .MinSize(wxSize(300, -1))
            .CaptionVisible(true)
            .DockFixed(false)
            .CloseButton(true)
            .DestroyOnClose(false)
            .Caption("Palette");
        manager->AddPane(palettePanel, info);
    }
    if (paneIds.has(PaneEnum::RENDER_PARAMS)) {
        info.Left()
            .MinSize(wxSize(300, -1))
            .CaptionVisible(true)
            .DockFixed(false)
            .CloseButton(true)
            .DestroyOnClose(false)
            .Caption("Visualization");
        manager->AddPane(visBar, info);
    }
    if (paneIds.has(PaneEnum::STATS)) {
        statsBar = createStatsBar();
        info.Right()
            .MinSize(wxSize(300, -1))
            .CaptionVisible(true)
            .DockFixed(false)
            .CloseButton(true)
            .DestroyOnClose(false)
            .Caption("Run statistics");
        manager->AddPane(statsBar, info);
    }
    if (paneIds.has(PaneEnum::PLOTS)) {
        wxPanel* plotBar = createPlotBar();
        info.Right()
            .MinSize(wxSize(300, -1))
            .CaptionVisible(true)
            .DockFixed(false)
            .CloseButton(true)
            .DestroyOnClose(false)
            .Caption("Plots");
        manager->AddPane(plotBar, info);
    }
    if (paneIds.has(PaneEnum::PARTICLE_DATA)) {
        wxPanel* probeBar = createProbeBar();
        info.Right()
            .MinSize(wxSize(300, -1))
            .CaptionVisible(true)
            .DockFixed(false)
            .CloseButton(true)
            .DestroyOnClose(false)
            .Caption("Particle data");
        manager->AddPane(probeBar, info);
    }

    manager->Update();
}

RunPage::~RunPage() {
    manager->UnInit();
    manager = nullptr;
}


const wxSize buttonSize(250, -1);
const wxSize spinnerSize(100, -1);
const int boxPadding = 10;

wxWindow* RunPage::createParticleBox(wxPanel* parent) {
    wxStaticBox* particleBox = new wxStaticBox(parent, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 118));

    wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* cutoffSizer = new wxBoxSizer(wxHORIZONTAL);
    cutoffSizer->AddSpacer(boxPadding);
    wxStaticText* text = new wxStaticText(particleBox, wxID_ANY, "Cutoff [km]");
    cutoffSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float cutoff = gui.get<Float>(GuiSettingsId::CAMERA_ORTHO_CUTOFF) * 1.e-3_f;

    FloatTextCtrl* cutoffCtrl = new FloatTextCtrl(particleBox, cutoff, Interval(0, LARGE));
    cutoffCtrl->onValueChanged = [this](const double value) {
        this->updateCutoff(value * 1.e3_f);
        return true;
    };
    cutoffCtrl->SetToolTip(
        "Specifies the cutoff distance in kilometers for rendering particles. When set to a positive number, "
        "only particles in a layer of specified thickness are rendered. Zero means all particles are "
        "rendered.");
    cutoffSizer->Add(cutoffCtrl, 1, wxALIGN_CENTER_VERTICAL);
    cutoffSizer->AddSpacer(boxPadding);
    boxSizer->Add(cutoffSizer);

    wxBoxSizer* particleSizeSizer = new wxBoxSizer(wxHORIZONTAL);
    particleSizeSizer->AddSpacer(boxPadding);
    text = new wxStaticText(particleBox, wxID_ANY, "Particle radius");
    particleSizeSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float radius = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    FloatTextCtrl* particleSizeCtrl = new FloatTextCtrl(particleBox, radius, Interval(1.e-3_f, 1.e3_f));
    particleSizeCtrl->SetToolTip(
        "Multiplier of a particle radius. Must be set to 1 to get the actual size of particles in N-body "
        "simulations.");
    particleSizeCtrl->onValueChanged = [this](const Float value) {
        gui.set(GuiSettingsId::PARTICLE_RADIUS, value);
        controller->refresh();
        return true;
    };
    particleSizeSizer->Add(particleSizeCtrl, 1, wxALIGN_CENTER_VERTICAL);
    particleSizeSizer->AddSpacer(boxPadding);
    boxSizer->Add(particleSizeSizer);

    wxBoxSizer* ghostSizer = new wxBoxSizer(wxHORIZONTAL);
    ghostSizer->AddSpacer(boxPadding);
    wxCheckBox* ghostBox = new wxCheckBox(particleBox, wxID_ANY, "Show ghosts");
    ghostBox->SetValue(gui.get<bool>(GuiSettingsId::RENDER_GHOST_PARTICLES));
    ghostSizer->Add(ghostBox);
    boxSizer->Add(ghostSizer);

    wxBoxSizer* aaSizer = new wxBoxSizer(wxHORIZONTAL);
    aaSizer->AddSpacer(boxPadding);
    wxCheckBox* aaBox = new wxCheckBox(particleBox, wxID_ANY, "Anti-aliasing");
    aaBox->SetValue(gui.get<bool>(GuiSettingsId::ANTIALIASED));
    aaBox->SetToolTip(
        "If checked, particles are drawn with anti-aliasing, creating smoother image, but it also takes "
        "longer to render it.");
    aaSizer->Add(aaBox);
    boxSizer->Add(aaSizer);

    particleBox->SetSizer(boxSizer);

    ghostBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        gui.set(GuiSettingsId::RENDER_GHOST_PARTICLES, value);
        controller->tryRedraw();
    });
    aaBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();
        gui.set(GuiSettingsId::ANTIALIASED, value);
        controller->refresh();
    });

    return particleBox;
}

wxWindow* RunPage::createRaymarcherBox(wxPanel* parent) {
    wxStaticBox* raytraceBox = new wxStaticBox(parent, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 125));
    wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* levelSizer = new wxBoxSizer(wxHORIZONTAL);
    levelSizer->AddSpacer(boxPadding);
    wxStaticText* text = new wxStaticText(raytraceBox, wxID_ANY, "Surface level");
    levelSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float level = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    FloatTextCtrl* levelCtrl = new FloatTextCtrl(raytraceBox, level, Interval(0._f, 10._f));
    levelCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_LEVEL, value);
        controller->refresh();
        return true;
    };
    levelSizer->Add(levelCtrl, 1, wxALIGN_CENTER_VERTICAL);
    levelSizer->AddSpacer(boxPadding);
    boxSizer->Add(levelSizer);

    wxBoxSizer* sunlightSizer = new wxBoxSizer(wxHORIZONTAL);
    sunlightSizer->AddSpacer(boxPadding);
    text = new wxStaticText(raytraceBox, wxID_ANY, "Sunlight");
    sunlightSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float sunlight = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
    FloatTextCtrl* sunlightCtrl = new FloatTextCtrl(raytraceBox, sunlight, Interval(0._f, 100._f));
    sunlightCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_SUN_INTENSITY, value);
        controller->refresh();
        return true;
    };
    sunlightSizer->Add(sunlightCtrl, 1, wxALIGN_CENTER_VERTICAL);
    sunlightSizer->AddSpacer(boxPadding);
    boxSizer->Add(sunlightSizer);

    wxBoxSizer* ambientSizer = new wxBoxSizer(wxHORIZONTAL);
    ambientSizer->AddSpacer(boxPadding);
    text = new wxStaticText(raytraceBox, wxID_ANY, "Ambient");
    ambientSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float ambient = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
    FloatTextCtrl* ambientCtrl = new FloatTextCtrl(raytraceBox, ambient, Interval(0._f, 100._f));
    ambientCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_AMBIENT, value);
        controller->refresh();
        return true;
    };
    ambientSizer->Add(ambientCtrl, 1, wxALIGN_CENTER_VERTICAL);
    ambientSizer->AddSpacer(boxPadding);
    boxSizer->Add(ambientSizer);

    wxBoxSizer* emissionSizer = new wxBoxSizer(wxHORIZONTAL);
    emissionSizer->AddSpacer(boxPadding);
    text = new wxStaticText(raytraceBox, wxID_ANY, "Emission");
    emissionSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float emission = gui.get<Float>(GuiSettingsId::SURFACE_EMISSION);
    FloatTextCtrl* emissionCtrl = new FloatTextCtrl(raytraceBox, emission, Interval(0._f, 100._f));
    emissionCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::SURFACE_EMISSION, value);
        controller->refresh();
        return true;
    };
    emissionSizer->Add(emissionCtrl, 1, wxALIGN_CENTER_VERTICAL);
    emissionSizer->AddSpacer(boxPadding);
    boxSizer->Add(emissionSizer);

    raytraceBox->SetSizer(boxSizer);
    return raytraceBox;
}

wxWindow* RunPage::createVolumeBox(wxPanel* parent) {
    wxStaticBox* volumeBox = new wxStaticBox(parent, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 100));
    wxBoxSizer* boxSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* emissionSizer = new wxBoxSizer(wxHORIZONTAL);
    emissionSizer->AddSpacer(boxPadding);
    wxStaticText* text = new wxStaticText(volumeBox, wxID_ANY, "Emission [km^-1]");
    emissionSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float emission = gui.get<Float>(GuiSettingsId::VOLUME_EMISSION);
    FloatTextCtrl* emissionCtrl = new FloatTextCtrl(volumeBox, emission * 1.e3_f, Interval(0._f, 1.e8_f));
    emissionCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        // value in spinner is in [km^-1]
        gui.set(GuiSettingsId::VOLUME_EMISSION, value / 1.e3_f);
        controller->refresh();
        return true;
    };
    emissionSizer->Add(emissionCtrl, 1, wxALIGN_CENTER_VERTICAL);
    emissionSizer->AddSpacer(boxPadding);
    boxSizer->Add(emissionSizer);

    wxBoxSizer* absorptionSizer = new wxBoxSizer(wxHORIZONTAL);
    absorptionSizer->AddSpacer(boxPadding);
    text = new wxStaticText(volumeBox, wxID_ANY, "Absorption [km^-1]");
    absorptionSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float absorption = gui.get<Float>(GuiSettingsId::VOLUME_ABSORPTION);
    FloatTextCtrl* absorptionCtrl = new FloatTextCtrl(volumeBox, absorption * 1.e3_f, Interval(0._f, 1.e8_f));
    absorptionCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        // value in spinner is in [km^-1]
        gui.set(GuiSettingsId::VOLUME_ABSORPTION, value / 1.e3_f);
        controller->refresh();
        return true;
    };
    absorptionSizer->Add(absorptionCtrl, 1, wxALIGN_CENTER_VERTICAL);
    absorptionSizer->AddSpacer(boxPadding);
    boxSizer->Add(absorptionSizer);

    wxBoxSizer* factorSizer = new wxBoxSizer(wxHORIZONTAL);
    factorSizer->AddSpacer(boxPadding);
    text = new wxStaticText(volumeBox, wxID_ANY, "Compression");
    factorSizer->Add(text, 10, wxALIGN_CENTER_VERTICAL);
    const Float factor = gui.get<Float>(GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR);
    FloatTextCtrl* factorCtrl = new FloatTextCtrl(volumeBox, factor, Interval(1.e-6_f, 1.e6_f));
    factorCtrl->onValueChanged = [this](const Float value) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR, value);
        controller->refresh();
        return true;
    };
    factorSizer->Add(factorCtrl, 1, wxALIGN_CENTER_VERTICAL);
    factorSizer->AddSpacer(boxPadding);
    boxSizer->Add(factorSizer);

    volumeBox->SetSizer(boxSizer);
    return volumeBox;
}

static void enableRecursive(wxWindow* window, const bool enable) {
    window->Enable(enable);
    for (wxWindow* child : window->GetChildren()) {
        enableRecursive(child, enable);
    }
}

wxPanel* RunPage::createVisBar() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    wxPanel* visbarPanel = new wxPanel(this);
    visbarPanel->SetLabel("Visualization");

    wxBoxSizer* visbarSizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->AddStretchSpacer(1);
    wxButton* resetView = new wxButton(visbarPanel, wxID_ANY, "Reset view");
    resetView->SetToolTip("Resets the camera rotation.");
    resetView->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        pane->resetView();
        AutoPtr<ICamera> camera = controller->getCurrentCamera();
        camera->transform(AffineMatrix::identity());
        controller->refresh(std::move(camera));
    });
    buttonSizer->Add(resetView);

    wxButton* refresh = new wxButton(visbarPanel, wxID_ANY, "Refresh");
    refresh->SetToolTip("Updates the particle order and repaints the current view");
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        if (!controller->tryRedraw()) {
            /// \todo allowing refreshing without camera parameter?
            AutoPtr<ICamera> camera = controller->getCurrentCamera();
            controller->refresh(std::move(camera));
            controller->redrawOnNextTimeStep();
        }
    });
    buttonSizer->Add(refresh);

    wxButton* snap = new wxButton(visbarPanel, wxID_ANY, "Save image");
    snap->SetToolTip("Saves the currently rendered image.");
    buttonSizer->Add(snap);
    snap->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doSaveFileDialog("Save image", { { "PNG image", "png" } });
        if (!path) {
            return;
        }
        const wxBitmap& bitmap = controller->getRenderedBitmap();
        saveToFile(bitmap, path.value());
    });
    buttonSizer->AddStretchSpacer(1);

    visbarSizer->Add(buttonSizer, 0, wxALIGN_CENTER_HORIZONTAL);
    visbarSizer->AddSpacer(10);

    wxCheckBox* autoRefresh = new wxCheckBox(visbarPanel, wxID_ANY, "Refresh on timestep");
    autoRefresh->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::REFRESH_ON_TIMESTEP, evt.IsChecked());
    });
    autoRefresh->SetValue(gui.get<bool>(GuiSettingsId::REFRESH_ON_TIMESTEP));
    autoRefresh->SetToolTip(
        "When checked, the image is updated on every timestep, otherwise the image is only updated when "
        "pressing the 'Refresh' button. Note that repainting the image on every timestep may decrease "
        "the performance of the code.");
    visbarSizer->Add(autoRefresh);

    wxCheckBox* autoCamera = new wxCheckBox(visbarPanel, wxID_ANY, "Auto-zoom");
    autoCamera->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        GuiSettings& gui = controller->getParams();
        gui.set(GuiSettingsId::CAMERA_AUTOSETUP, evt.IsChecked());
    });
    autoCamera->SetValue(gui.get<bool>(GuiSettingsId::CAMERA_AUTOSETUP));
    autoCamera->SetToolTip(
        "When checked, parameters of the camera (position, field of view, etc.) are automatically adjusted "
        "during the simulation.");
    visbarSizer->Add(autoCamera);
    visbarSizer->AddSpacer(10);


    /*wxBoxSizer* colorSizer = new wxBoxSizer(wxHORIZONTAL);
    colorSizer->Add(new wxStaticText(visbarPanel, wxID_ANY, "Background: "), 10, wxALIGN_CENTER_VERTICAL);
    wxColourPickerCtrl* picker = new wxColourPickerCtrl(visbarPanel, wxID_ANY);
    picker->SetColour(wxColour(controller->getParams().get<Rgba>(GuiSettingsId::BACKGROUND_COLOR)));
    colorSizer->Add(picker, 1, wxALIGN_CENTER_VERTICAL, 5);
    wxCheckBox* transparentBox = new wxCheckBox(visbarPanel, wxID_ANY, "Transparent");
    picker->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent& evt) {
        GuiSettings& gui = controller->getParams();
        Rgba newColor(evt.GetColour());
        const Rgba currentColor = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
        newColor.a() = currentColor.a();
        gui.set(GuiSettingsId::BACKGROUND_COLOR, newColor);
        controller->tryRedraw();
    });
    transparentBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& evt) {
        GuiSettings& gui = controller->getParams();
        Rgba color = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
        color.a() = evt.IsChecked() ? 0.f : 1.f;
        gui.set(GuiSettingsId::BACKGROUND_COLOR, color);
        controller->tryRedraw();
    });
    colorSizer->Add(transparentBox, 1, wxALIGN_CENTER_VERTICAL, 5);
    visbarSizer->Add(colorSizer);
    visbarSizer->AddSpacer(10);*/

    wxBoxSizer* quantitySizer = new wxBoxSizer(wxHORIZONTAL);

    quantitySizer->AddSpacer(15);
    quantitySizer->Add(new wxStaticText(visbarPanel, wxID_ANY, "Quantity"), 10, wxALIGN_CENTER_VERTICAL);
    quantityBox = new ComboBox(visbarPanel, "", 160);
    quantityBox->SetToolTip(
        "Selects which quantity to visualize using associated color scale. Quantity values can be also "
        "obtained by left-clicking on a particle.");
    quantityBox->SetSelection(0);
    quantityBox->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const int idx = quantityBox->GetSelection();
        this->setColorizer(idx);
    });
    quantitySizer->Add(quantityBox, 1, wxALIGN_CENTER_VERTICAL, 5);
    quantitySizer->AddSpacer(13);

    visbarSizer->Add(quantitySizer);
    visbarSizer->AddSpacer(10);

    wxRadioButton* particleButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Particles", wxDefaultPosition, buttonSize, wxRB_GROUP);
    particleButton->SetToolTip("Render individual particles with optional smoothing.");
    visbarSizer->Add(particleButton);
    wxWindow* particleBox = this->createParticleBox(visbarPanel);
    visbarSizer->Add(particleBox, 0, wxALL, 5);
    visbarSizer->AddSpacer(10);


    wxRadioButton* surfaceButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Raymarched surface", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(surfaceButton);
    wxWindow* raytracerBox = this->createRaymarcherBox(visbarPanel);
    visbarSizer->Add(raytracerBox, 0, wxALL, 5);
    visbarSizer->AddSpacer(10);

    wxRadioButton* volumeButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Volumetric raytracer", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(volumeButton);
    wxWindow* volumeBox = this->createVolumeBox(visbarPanel);
    visbarSizer->Add(volumeBox, 0, wxALL, 5);
    visbarSizer->AddSpacer(10);

    visbarSizer->AddStretchSpacer(1);

    /*wxRadioButton* contourButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Iso-lines", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(contourButton);
    wxWindow* contourBox = this->createContourBox(visbarPanel);
    visbarSizer->Add(contourBox, 0, wxALL, 5);
    visbarSizer->AddSpacer(6);

    wxRadioButton* meshButton =
        new wxRadioButton(visbarPanel, wxID_ANY, "Surface mesh", wxDefaultPosition, buttonSize, 0);
    visbarSizer->Add(meshButton);
    visbarSizer->AddSpacer(6);*/


    auto enableControls = [=](int renderIdx) {
        enableRecursive(particleBox, renderIdx == 0);
        enableRecursive(raytracerBox, renderIdx == 1);
        enableRecursive(volumeBox, renderIdx == 2);
    };
    enableControls(0);

    particleButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        controller->setRenderer(makeAuto<ParticleRenderer>(gui));
        enableControls(0);
    });
    /*meshButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());
        controller->setRenderer(makeAuto<MeshRenderer>(scheduler, gui));
        enableControls(3);
    });
    contourButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());
        controller->setRenderer(makeAuto<ContourRenderer>(scheduler, gui));
        enableControls(2);
    });*/
    surfaceButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        try {
            SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());
            controller->setRenderer(makeAuto<RayMarcher>(scheduler, gui));
            enableControls(1);
        } catch (const std::exception& e) {
            wxMessageBox(std::string("Cannot initialize raytracer.\n\n") + e.what(), "Error", wxOK);

            // switch to particle renderer (fallback option)
            particleButton->SetValue(true);
            controller->setRenderer(makeAuto<ParticleRenderer>(gui));
            enableControls(0);
        }
    });
    volumeButton->Bind(wxEVT_RADIOBUTTON, [=](wxCommandEvent& UNUSED(evt)) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());
        GuiSettings volumeGui = gui;
        volumeGui.set(GuiSettingsId::COLORMAP_TYPE, ColorMapEnum::LOGARITHMIC);
        controller->setRenderer(makeAuto<VolumeRenderer>(scheduler, volumeGui));
        enableControls(2);
    });

    visbarPanel->SetSizer(visbarSizer);
    return visbarPanel;
}

void RunPage::updateCutoff(const double cutoff) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    gui.set(GuiSettingsId::CAMERA_ORTHO_CUTOFF, Float(cutoff));
    // Note that we have to get camera from pane, not controller, as pane camera is always the one being
    // modified and fed to controller. Using controller's camera would cause cutoff to be later overriden by
    // the camera from pane.
    ICamera& camera = pane->getCamera();
    camera.setCutoff(cutoff > 0. ? Optional<float>(float(cutoff)) : NOTHING);
    controller->refresh(camera.clone());
    // needs to re-initialize the renderer
    controller->tryRedraw();
}

wxPanel* RunPage::createProbeBar() {
    wxPanel* sidebarPanel = new wxPanel(this);
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);
    probe = new ParticleProbe(sidebarPanel, wxSize(300, 155));
    sidebarSizer->Add(probe.get(), 1, wxALIGN_TOP | wxEXPAND);

    sidebarPanel->SetSizerAndFit(sidebarSizer);
    return sidebarPanel;
}

wxPanel* RunPage::createPlotBar() {
    wxPanel* sidebarPanel = new wxPanel(this);
    wxBoxSizer* sidebarSizer = new wxBoxSizer(wxVERTICAL);

    SharedPtr<Array<PlotData>> list = makeShared<Array<PlotData>>(getPlotList(gui));
    for (const auto& plotData : *list) {
        plots.push(plotData.plot);
    }

    TicsParams tics;
    tics.minCnt = 2;
    tics.digits = 1;
    plotViews.push(new PlotView(sidebarPanel, wxSize(300, 200), wxSize(10, 10), list, 0, tics));
    sidebarSizer->Add(plotViews.back().get(), 1, wxALIGN_TOP | wxEXPAND);
    sidebarSizer->AddSpacer(5);

    plotViews.push(new PlotView(sidebarPanel, wxSize(300, 200), wxSize(10, 10), list, 1, tics));
    sidebarSizer->Add(plotViews.back().get(), 1, wxALIGN_TOP | wxEXPAND);

    sidebarPanel->SetSizerAndFit(sidebarSizer);
    return sidebarPanel;
}

wxPanel* RunPage::createStatsBar() {
    wxPanel* statsPanel = new wxPanel(this);
    wxBoxSizer* statsSizer = new wxBoxSizer(wxVERTICAL);

    wxFont font = wxSystemSettings::GetFont(wxSYS_SYSTEM_FONT);
    font.Scale(0.95f);
    statsPanel->SetFont(font);

    statsText = new wxTextCtrl(
        statsPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
    this->makeStatsText(0, 0, Statistics{});

    statsSizer->Add(statsText, 1, wxEXPAND | wxALL, 5);
    statsPanel->SetSizer(statsSizer);

    return statsPanel;
}

template <typename TValue>
static void printStat(wxTextCtrl* text,
    const Statistics& stats,
    const std::string& desc,
    const StatisticsId id,
    const std::string units = "") {
    if (stats.has(id)) {
        *text << desc << stats.get<TValue>(id) << units << "\n";
    }
}

void RunPage::makeStatsText(const Size particleCnt, const Size pointCnt, const Statistics& stats) {
    statsText->Clear();
    *statsText << " - particles: ";
    if (particleCnt > 0) {
        *statsText << int(particleCnt) << "\n";
    } else {
        *statsText << "N/A\n";
    }

    if (pointCnt > 0) {
        *statsText << " - attractors: " << int(pointCnt) << "\n";
    }

    printStat<Float>(statsText, stats, " - run time:  ", StatisticsId::RUN_TIME, "s");
    printStat<Float>(statsText, stats, " - timestep:  ", StatisticsId::TIMESTEP_VALUE, "s");

    if (stats.has(StatisticsId::TIMESTEP_CRITERION)) {
        CriterionId id = stats.get<CriterionId>(StatisticsId::TIMESTEP_CRITERION);
        std::stringstream ss;
        if (id == CriterionId::DERIVATIVE) {
            ss << stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY);
        } else {
            ss << id;
        }
        *statsText << "    * set by: " << ss.str() << "\n";
    }

    printStat<int>(statsText, stats, " - time spent:  ", StatisticsId::TIMESTEP_ELAPSED, "ms");
    printStat<int>(statsText, stats, "    * SPH evaluation: ", StatisticsId::SPH_EVAL_TIME, "ms");
    printStat<int>(statsText, stats, "    * gravity evaluation: ", StatisticsId::GRAVITY_EVAL_TIME, "ms");
    printStat<int>(statsText, stats, "    * collision evaluation: ", StatisticsId::COLLISION_EVAL_TIME, "ms");
    printStat<int>(statsText, stats, "    * tree construction:    ", StatisticsId::GRAVITY_BUILD_TIME, "ms");
    printStat<int>(
        statsText, stats, "    * visualization:        ", StatisticsId::POSTPROCESS_EVAL_TIME, "ms");

    printStat<int>(statsText, stats, " - collisions:  ", StatisticsId::TOTAL_COLLISION_COUNT);
    printStat<int>(statsText, stats, "    * bounces:  ", StatisticsId::BOUNCE_COUNT);
    printStat<int>(statsText, stats, "    * mergers:  ", StatisticsId::MERGER_COUNT);
    printStat<int>(statsText, stats, "    * breakups: ", StatisticsId::BREAKUP_COUNT);
    printStat<int>(statsText, stats, " - overlaps:    ", StatisticsId::OVERLAP_COUNT);
    printStat<int>(statsText, stats, " - aggregates:  ", StatisticsId::AGGREGATE_COUNT);
}

void RunPage::setColorizer(const Size idx) {
    // do this even if idx==selectedIdx, we might change the colorizerList
    // (weird behavior, but it will do for now)
    controller->setColorizer(colorizerList[idx]);
    if (idx == selectedIdx) {
        return;
    }
    Optional<Palette> palette = colorizerList[idx]->getPalette();
    if (palettePanel && palette) {
        palettePanel->setPalette(palette.value());
    }
    this->replaceQuantityBar(idx);
    selectedIdx = idx;
}

void RunPage::addComponentIdBar(wxWindow* parent, wxSizer* sizer, SharedPtr<IColorizer> colorizer) {
    sizer->AddSpacer(5);
    RawPtr<ComponentIdColorizer> componentId = dynamicCast<ComponentIdColorizer>(colorizer.get());

    wxBoxSizer* seedSizer = new wxBoxSizer(wxHORIZONTAL);
    seedSizer->Add(new wxStaticText(parent, wxID_ANY, "Seed"), 0, wxALIGN_CENTER_VERTICAL);
    wxSpinCtrl* seedSpinner = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, spinnerSize);
    seedSizer->Add(seedSpinner, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(seedSizer);
    sizer->AddSpacer(15);

    wxRadioButton* overlapButton = new wxRadioButton(
        parent, wxID_ANY, "Connected particles", wxDefaultPosition, wxSize(-1, 25), wxRB_GROUP);
    sizer->Add(overlapButton);

    wxRadioButton* boundButton =
        new wxRadioButton(parent, wxID_ANY, "Bound particles", wxDefaultPosition, wxSize(-1, 25), 0);
    sizer->Add(boundButton);

    sizer->AddSpacer(15);
    wxCheckBox* highlightBox = new wxCheckBox(parent, wxID_ANY, "Highlight component");
    highlightBox->SetValue(bool(componentId->getHighlightIdx()));
    sizer->Add(highlightBox);

    wxBoxSizer* highlightSizer = new wxBoxSizer(wxHORIZONTAL);
    highlightSizer->AddSpacer(30);
    wxStaticText* text = new wxStaticText(parent, wxID_ANY, "Index");
    highlightSizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);
    wxSpinCtrl* highlightIndex = new wxSpinCtrl(parent, wxID_ANY, "", wxDefaultPosition, spinnerSize);
    highlightIndex->SetValue(componentId->getHighlightIdx().valueOr(0));
    highlightIndex->Enable(highlightBox->GetValue());
    highlightSizer->Add(highlightIndex);
    sizer->Add(highlightSizer);

    overlapButton->Bind(wxEVT_RADIOBUTTON, [this, componentId, colorizer](wxCommandEvent& UNUSED(evt)) {
        componentId->setConnectivity(Post::ComponentFlag::SORT_BY_MASS | Post::ComponentFlag::OVERLAP);
        controller->setColorizer(colorizer);
    });
    boundButton->Bind(wxEVT_RADIOBUTTON, [this, componentId, colorizer](wxCommandEvent& UNUSED(evt)) {
        componentId->setConnectivity(
            Post::ComponentFlag::SORT_BY_MASS | Post::ComponentFlag::ESCAPE_VELOCITY);
        controller->setColorizer(colorizer);
    });
    seedSpinner->Bind(wxEVT_SPINCTRL, [this, componentId, colorizer](wxSpinEvent& evt) {
        const int seed = evt.GetValue();
        componentId->setSeed(seed);
        controller->setColorizer(colorizer);
    });

    highlightBox->Bind(wxEVT_CHECKBOX, [this, colorizer, componentId, highlightIndex](wxCommandEvent& evt) {
        const bool value = evt.IsChecked();

        SPH_ASSERT(componentId);
        if (value) {
            componentId->setHighlightIdx(highlightIndex->GetValue());
        } else {
            componentId->setHighlightIdx(NOTHING);
        }
        highlightIndex->Enable(value);
        /// \todo this causes rebuild of colorizer, which is very inefficient, there should be some concept of
        /// validity that would tell whether it is necessary or not to rebuild it
        controller->setColorizer(colorizer);
    });
    highlightIndex->Bind(wxEVT_SPINCTRL, [this, componentId, colorizer](wxSpinEvent& evt) {
        // this is already executed on main thread, but we query it anyway to avoid spinner getting stuck
        executeOnMainThread([this, componentId, colorizer, evt] {
            const int index = evt.GetValue();
            componentId->setHighlightIdx(index);
            /// \todo also unnecessary
            controller->setColorizer(colorizer);
        });
    });
}

void RunPage::replaceQuantityBar(const Size idx) {
    // so far only needed for component id, so it is hacked like this
    SharedPtr<IColorizer> newColorizer = colorizerList[idx];
    bool panelExists = bool(wxWeakRef<wxPanel>(quantityPanel));

    /// \todo implement SharedPtr dynamicCast
    if (!dynamicCast<ComponentIdColorizer>(newColorizer.get())) {
        manager->GetPane(quantityPanel).Hide();
        manager->Update();
        return;
    }

    if (panelExists) {
        manager->GetPane(quantityPanel).Show();
        manager->Update();
        return;
    }

    quantityPanel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    // sizer->Add(new wxStaticText(quantityPanel, wxID_ANY, newColorizer->name()));
    // wxBoxSizer* offsetSizer = new wxBoxSizer(wxHORIZONTAL);
    // offsetSizer->AddSpacer(25);
    // sizer->Add(offsetSizer);
    // wxBoxSizer* actSizer = new wxBoxSizer(wxVERTICAL);
    addComponentIdBar(quantityPanel, sizer, newColorizer);
    // offsetSizer->Add(sizer);
    quantityPanel->SetSizerAndFit(sizer);

    wxAuiPaneInfo info;
    info.Right()
        .Position(0)
        .MinSize(wxSize(300, -1))
        .CaptionVisible(true)
        .DockFixed(false)
        .CloseButton(true)
        .DestroyOnClose(true)
        .Caption("Components");
    manager->AddPane(quantityPanel, info);
    manager->Update();
}


void RunPage::setProgress(const Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    progressBar->update(stats);

    /// \todo check if IsShown works if hidden by AUI
    if (timelineBar->IsShown() && stats.has(StatisticsId::INDEX)) {
        timelineBar->setFrame(stats.get<int>(StatisticsId::INDEX));
    }
}

void RunPage::newPhase(const std::string& className, const std::string& instanceName) {
    progressBar->onRunStart(className, instanceName);
}

void RunPage::refresh() {
    pane->Refresh();
}

void RunPage::showTimeLine(const bool show) {
    wxAuiPaneInfo& timelineInfo = manager->GetPane(timelineBar);
    wxAuiPaneInfo& progressInfo = manager->GetPane(progressBar);
    wxAuiPaneInfo& statsInfo = manager->GetPane(statsBar);

    if (!timelineInfo.IsShown()) {
        timelineInfo.Show(show);
        progressInfo.Show(!show);
        statsInfo.Show(!show);
        manager->Update();
    }
}

void RunPage::runStarted(const Storage& storage, const Path& path) {
    Statistics dummy;
    pane->onTimeStep(storage, dummy);

    const Size particleCnt = storage.getParticleCnt();
    const Size pointCnt = storage.getAttractors().size();
    executeOnMainThread([this, particleCnt, pointCnt] {
        Statistics dummyStats;
        this->makeStatsText(particleCnt, pointCnt, dummyStats);
    });

    if (!path.empty()) {
        timelineBar->update(path);
    }

    for (auto plot : plots) {
        plot->clear();
    }
}

void RunPage::onTimeStep(const Storage& storage, const Statistics& stats) {
    // this is called from run thread (NOT main thread)

    // limit the refresh rate to avoid blocking the main thread
    if (statsText && statsTimer.elapsed(TimerUnit::MILLISECOND) > 100) {
        const Size particleCnt = storage.getParticleCnt();
        const Size pointCnt = storage.getAttractors().size();
        executeOnMainThread([this, stats, particleCnt, pointCnt] { //
            this->makeStatsText(particleCnt, pointCnt, stats);
        });
        statsTimer.restart();
    }

    pane->onTimeStep(storage, stats);

    if (selectedParticlePlot) {
        selectedParticlePlot->selectParticle(controller->getSelectedParticle());

        /// \todo we should only touch colorizer from main thread!
        SharedPtr<IColorizer> colorizer = controller->getCurrentColorizer();
        // we need validity of arrayrefs only for the duration of this function, so weak reference is OK
        colorizer->initialize(storage, RefEnum::WEAK);
        selectedParticlePlot->setColorizer(colorizer);
    }

    if (storage.has(QuantityId::MASS)) {
        // skip plots if we don't have mass, for simplicity; this can be generalized if needed
        for (auto plot : plots) {
            plot->onTimeStep(storage, stats);
        }

        executeOnMainThread([this] {
            for (auto view : plotViews) {
                view->Refresh();
            }
        });
    }
}

void RunPage::onRunEnd() {
    progressBar->onRunEnd();
    this->onStopped();
}

void RunPage::setColorizerList(Array<SharedPtr<IColorizer>>&& colorizers) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    colorizerList = std::move(colorizers);
    wxArrayString items;
    for (auto& e : colorizerList) {
        items.Add(e->name().c_str());
    }
    quantityBox->Set(items);
    const Size actSelectedIdx = (selectedIdx < colorizerList.size()) ? selectedIdx : 0;
    quantityBox->SetSelection(actSelectedIdx);
}

void RunPage::setSelectedParticle(const Particle& particle, const Rgba color) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->update(particle, color);
}

void RunPage::deselectParticle() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    probe->clear();
}

wxSize RunPage::getCanvasSize() const {
    const wxSize size = pane->GetSize();
    return wxSize(max(size.x, 1), max(size.y, 1));
}

bool RunPage::isRunning() const {
    return controller->isRunning();
}

void RunPage::stop() {
    controller->stop();
}

void RunPage::quit() {
    controller->quit(true);
}

NAMESPACE_SPH_END
