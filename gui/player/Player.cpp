#include "gui/player/Player.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/Uvw.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "objects/Exceptions.h"
#include "objects/utility/StringUtils.h"
#include "thread/CheckFunction.h"
#include "timestepping/ISolver.h"
#include <wx/dcclient.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

RunPlayer::RunPlayer(const Path& fileMask)
    : fileMask(fileMask) {}

/// \todo possibly move to Factory
static AutoPtr<IInput> getInput(const Path& path) {
    if (path.extension() == Path("ssf")) {
        return makeAuto<BinaryInput>();
    } else if (path.extension() == Path("scf")) {
        return makeAuto<CompressedInput>();
    } else {
        std::string str = path.native();
        if (str.substr(str.size() - 3) == ".bt") {
            return makeAuto<PkdgravInput>();
        }
    }
    throw InvalidSetup("Unknown file type: " + path.native());
}

static Size getFileCount(const Path& pathMask) {
    OutputFile of(pathMask);
    if (!of.hasWildcard()) {
        return FileSystem::pathExists(pathMask) ? 1 : 0;
    }
    Size cnt = 0;
    Statistics stats;
    while (true) {
        Path path = of.getNextPath(stats);
        if (FileSystem::pathExists(path)) {
            ++cnt;
        } else {
            break;
        }
    }
    return cnt;
}

void RunPlayer::setUp() {
    logger = makeAuto<StdOutLogger>();

    files = OutputFile(fileMask);
    fileCnt = getFileCount(fileMask);

    if (fileCnt > 1) {
        logger->write("Loading sequence of ", fileCnt, " files");
    }

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    const Path firstPath = files.getNextPath(stats);
    if (!FileSystem::pathExists(firstPath)) {
        throw InvalidSetup("Cannot locate file " + firstPath.native());
    }

    storage = makeShared<Storage>();
    AutoPtr<IInput> input = getInput(firstPath);
    Outcome result = input->load(firstPath, *storage, stats);
    if (!result) {
        throw InvalidSetup("Cannot load the run state file " + firstPath.native());
    } else {
        loadedTime = stats.get<Float>(StatisticsId::RUN_TIME);
    }
    logger->write("Loaded ", storage->getParticleCnt(), " particles");

    // setupUvws(*storage);

    callbacks = makeAuto<GuiCallbacks>(*controller);

    // dummy solver, used to avoid calling create on stuff
    struct PlayerSolver : public ISolver {
        virtual void integrate(Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

        virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
    };
    solver = makeAuto<PlayerSolver>();
}

void RunPlayer::run() {
    ASSERT(storage);
    setNullToDefaults();
    logger->write("Running:");

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, loadedTime);
    callbacks->onRunStart(*storage, stats);

    const Size stepMilliseconds = Size(1000._f / fps);
    for (Size i = 0; i < fileCnt; ++i) {
        Timer stepTimer;
        stats.set(StatisticsId::RELATIVE_PROGRESS, Float(i) / fileCnt);
        callbacks->onTimeStep(*storage, stats);

        if (i != fileCnt - 1) {
            storage = makeShared<Storage>();
            const Path path = files.getNextPath(stats);
            Timer loadTimer;
            AutoPtr<IInput> io = getInput(path);
            const Outcome result = io->load(path, *storage, stats);
            if (!result) {
                executeOnMainThread([path] {
                    wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                });
            }
            logger->write("Loaded ", path.native(), " in ", loadTimer.elapsed(TimerUnit::MILLISECOND), " ms");
            // setupUvws(*storage);

            const Size elapsed = stepTimer.elapsed(TimerUnit::MILLISECOND);
            if (elapsed < stepMilliseconds) {
                std::this_thread::sleep_for(std::chrono::milliseconds(stepMilliseconds - elapsed));
            }
        }

        if (callbacks->shouldAbortRun()) {
            break;
        }
    }
    if (fileCnt > 1) {
        logger->write("File sequence finished");
    } else {
        logger->write("File finished");
    }
    this->tearDownInternal(stats);
}

void RunPlayer::tearDown(const Statistics& UNUSED(stats)) {}

class TimeLinePanel : public wxPanel {
private:
    Path fileMask;
    Function<void(Path)> onFrameChanged;

    int fileCnt;
    int currentFrame = 0; /// \todo somehow propagate this from the simulation
    int mouseFrame = 0;

public:
    TimeLinePanel(wxWindow* parent, const Path inputFile, Function<void(Path)> onFrameChanged)
        : wxPanel(parent, wxID_ANY)
        , onFrameChanged(onFrameChanged)
        , fileCnt(int(fileCnt)) {

        OutputFile outputFile(inputFile);
        if (outputFile.hasWildcard()) {
            // already a mask
            fileMask = inputFile;
        } else {
            Optional<OutputFile> deducedFile = OutputFile::getMaskFromPath(inputFile);
            ASSERT(deducedFile); /// \todo message box
            fileMask = deducedFile->getMask();
            currentFrame = OutputFile::getDumpIdx(inputFile).valueOr(0);
        }
        fileCnt = getFileCount(fileMask);

        this->SetMinSize(wxSize(1024, 60));
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(TimeLinePanel::onPaint));
        this->Connect(wxEVT_MOTION, wxMouseEventHandler(TimeLinePanel::onMouseMotion));
        this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(TimeLinePanel::onLeftClick));
    }

private:
    int positionToFrame(const wxPoint position) const {
        wxSize size = this->GetSize();
        return int(roundf(float(position.x) * (fileCnt - 1) / size.x));
    }

    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        const wxSize size = dc.GetSize();
        /// \todo deduplicate
        Rgba backgroundColor = Rgba(this->GetParent()->GetBackgroundColour());
        wxBrush brush;
        brush.SetColour(wxColour(backgroundColor.darken(0.2_f)));
        dc.SetBrush(brush);
        dc.DrawRectangle(wxPoint(0, 0), size);

        wxPen pen = *wxBLACK_PEN;
        for (int i = 0; i < fileCnt; ++i) {
            if (i == currentFrame) {
                pen.SetColour(wxColour(255, 80, 0));
            } else if (i == mouseFrame) {
                pen.SetColour(wxColour(128, 128, 128));
            } else {
                pen.SetColour(*wxBLACK);
            }
            dc.SetPen(pen);
            const int x = i * size.x / (fileCnt - 1);
            dc.DrawLine(wxPoint(x, 0), wxPoint(x, size.y));
        }
    }

    void onMouseMotion(wxMouseEvent& evt) {
        mouseFrame = positionToFrame(evt.GetPosition());
        this->Refresh();
    }

    void onLeftClick(wxMouseEvent& evt) {
        currentFrame = positionToFrame(evt.GetPosition());

        OutputFile newFile(fileMask, currentFrame);
        Statistics stats;
        onFrameChanged(newFile.getNextPath(stats));
    }
};

class TimeLinePlugin : public IPluginControls {
private:
    Path fileMask;
    Function<void(Path)> onFrameChanged;

    TimeLinePanel* panel;

public:
    TimeLinePlugin(const Path& fileMask, Function<void(Path)> onFrameChanged)
        : fileMask(fileMask)
        , onFrameChanged(onFrameChanged) {}

    virtual void create(wxWindow* parent, wxSizer* sizer) override {
        /*wxSlider* slider = new wxSlider(parent, wxID_ANY, 0, 0, 100);
        slider->SetSize(wxSize(800, 100));
        sizer->Add(slider);*/
        panel = alignedNew<TimeLinePanel>(parent, fileMask, onFrameChanged);
        sizer->AddSpacer(5);
        sizer->Add(panel);
        sizer->AddSpacer(5);
    }

    virtual void statusChanges(const RunStatus UNUSED(newStatus)) override {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        panel->Refresh();
    }
};

bool App::OnInit() {
    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    Path fileMask;
    if (wxTheApp->argc == 1) {
        Optional<Path> selectedFile = doOpenFileDialog("Open file",
            { { "SPH state files", "ssf" },
                { "SPH compressed files", "scf" },
                { "Pkdgrav output files", "bt" } });
        if (!selectedFile) {
            return false;
        }
        fileMask = selectedFile.value();

    } else {
        fileMask = Path(std::string(wxTheApp->argv[1]));
    }

    RunTypeEnum runType = RunTypeEnum::SPH;

    OutputFile files(fileMask);
    Statistics stats;
    Path path = files.getNextPath(stats);
    BinaryInput input;
    if (Expected<BinaryInput::Info> info = input.getInfo(path)) {
        runType = info->runType.valueOr(RunTypeEnum::SPH);
    }
    const bool isNBody = (runType == RunTypeEnum::NBODY) || (runType == RunTypeEnum::RUBBLE_PILE);

    GuiSettings gui;
    gui.set(GuiSettingsId::ORTHO_FOV, 0._f)
        .set(GuiSettingsId::ORTHO_VIEW_CENTER, 0.5_f * Vector(768, 768, 0))
        .set(GuiSettingsId::VIEW_WIDTH, 1024)
        .set(GuiSettingsId::VIEW_HEIGHT, 768)
        .set(GuiSettingsId::IMAGES_WIDTH, 1024)
        .set(GuiSettingsId::IMAGES_HEIGHT, 768)
        .set(GuiSettingsId::WINDOW_WIDTH, 1630)
        .set(GuiSettingsId::WINDOW_HEIGHT, 768)
        .set(GuiSettingsId::WINDOW_TITLE,
            std::string("OpenSPH viewer - ") + path.native() + " (build: " + __DATE__ + ")")
        .set(GuiSettingsId::PARTICLE_RADIUS, isNBody ? 1._f : 0.35_f)
        .set(GuiSettingsId::SURFACE_LEVEL, 0.13_f)
        .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-1.e6_f, -1.5e6_f, 0._f)))
        .set(GuiSettingsId::SURFACE_SUN_INTENSITY, 0.7_f)
        .set(GuiSettingsId::SURFACE_AMBIENT, 0.3_f)
        .set(GuiSettingsId::SURFACE_RESOLUTION, 1e5_f)
        .set(GuiSettingsId::CAMERA, CameraEnum::ORTHO)
        //.set(GuiSettingsId::PERSPECTIVE_TRACKED_PARTICLE, 776703)
        //.set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(-3.e6_f, 4.e6_f, 0._f))
        //.set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(0._f, 2.e6_f, -4.e6_f))
        //.set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(0._f, 0._f, 0._f))
        .set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(-4.e4, -3.8e4_f, 0._f))
        .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(-4.e4, -3.8e4_f, 8.e5_f))
        //.set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(0._f, -1.e6_f, 0._f))

        .set(GuiSettingsId::PERSPECTIVE_CLIP_NEAR, EPS)
        .set(GuiSettingsId::BACKGROUND_COLOR, Vector(0._f))
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::ORTHO_CUTOFF, 0._f) // 10000._f)
        .set(GuiSettingsId::ORTHO_ZOFFSET, -1.e7_f)
        .set(GuiSettingsId::VIEW_GRID_SIZE, 0._f)
        .set(GuiSettingsId::RENDERER, RendererEnum::PARTICLE)
        .set(GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY, std::string(""))
        .set(GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY, std::string(""))
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 3)
        .set(GuiSettingsId::RAYTRACE_ITERATION_LIMIT, 50)
        .set(GuiSettingsId::IMAGES_WIDTH, 800)
        .set(GuiSettingsId::IMAGES_HEIGHT, 800)
        .set(GuiSettingsId::IMAGES_SAVE, false)
        .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
        .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"))
        .set(GuiSettingsId::IMAGES_TIMESTEP, 0._f)
        .set(GuiSettingsId::IMAGES_RENDERER, RendererEnum::PARTICLE)
        /*.set(GuiSettingsId::PALETTE_STRESS, Interval(1.e5_f, 3.e6_f))
        .set(GuiSettingsId::PALETTE_VELOCITY, Interval(1._f, 100._f))
        .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-5.e6_f, 5.e6_f))
        .set(GuiSettingsId::PALETTE_ENERGY, Interval(100._f, 5.e4_f))
        .set(GuiSettingsId::PALETTE_RADIUS, Interval(700._f, 3.e3_f))
        .set(GuiSettingsId::PALETTE_GRADV, Interval(0._f, 1.e-5_f))*/
        .set(GuiSettingsId::PLOT_INTEGRALS, PlotEnum::KINETIC_ENERGY)
        .set(GuiSettingsId::PLOT_OVERPLOT_SFD, std::string("/home/pavel/Dropbox/family.dat_hc"));

    if (runType == RunTypeEnum::NBODY) {
        gui.set(GuiSettingsId::PLOT_INTEGRALS,
            PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_ENERGY |
                PlotEnum::TOTAL_MOMENTUM | PlotEnum::TOTAL_ANGULAR_MOMENTUM | PlotEnum::PARTICLE_SFD);
    }
    //"projects/astro/asteroids/hygiea/main_belt_families_2018/10_Hygiea/"
    //"size_distribution/family.dat_hc"));


    if (fileMask.native().substr(fileMask.native().size() - 3) == ".bt") {
        // pkdgrav file, override some options
        gui.set(GuiSettingsId::ORTHO_FOV, 1.e6_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::PARTICLE_RADIUS, 1._f);
    }

    /*if (!OutputFile(fileMask).hasWildcard()) {
        Path name = fileMask.fileName();
        name.replaceExtension("png");
        gui.set(GuiSettingsId::IMAGES_NAME, name.native());
    }*/

    /*if (FileSystem::pathExists(Path("gui.sph"))) {
        gui.loadFromFile(Path("gui.sph"));
    } else {
        gui.saveToFile(Path("gui.sph"));
    }*/

    /// \todo this is ridiculous, but it will do for now
    auto callback = [this](const Path& newPath) {
        AutoPtr<RunPlayer> newRun = makeAuto<RunPlayer>(newPath);
        newRun->setController(controller.get());
        controller->start(std::move(newRun));
    };
    controller = makeAuto<Controller>(gui, makeAuto<TimeLinePlugin>(fileMask, callback));

    AutoPtr<RunPlayer> run = makeAuto<RunPlayer>(fileMask);
    run->setController(controller.get());
    controller->start(std::move(run));
    return true;
}

NAMESPACE_SPH_END
