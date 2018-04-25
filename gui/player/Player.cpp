#include "gui/player/Player.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/Uvw.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "objects/utility/StringUtils.h"
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

RunPlayer::RunPlayer(const Path& fileMask)
    : fileMask(fileMask) {}

static AutoPtr<IOutput> getOutput(const Path& path) {
    if (path.extension() == Path("ssf")) {
        return makeAuto<BinaryOutput>();
    } else {
        std::string str = path.native();
        if (str.substr(str.size() - 3) == ".bt") {
            return makeAuto<PkdgravOutput>();
        }
    }
    throw InvalidSetup("Unknown file type: " + path.native());
}

void RunPlayer::setUp() {
    files = OutputFile(fileMask);
    fileCnt = this->getFileCount(fileMask);

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    const Path firstPath = files.getNextPath(stats);
    if (!FileSystem::pathExists(firstPath)) {
        throw InvalidSetup("Cannot locate file " + firstPath.native());
    }

    storage = makeShared<Storage>();
    AutoPtr<IOutput> io = getOutput(firstPath);
    Outcome result = io->load(firstPath, *storage, stats);
    if (!result) {
        throw InvalidSetup("Cannot load the run state file " + firstPath.native());
    } else {
        loadedTime = stats.get<Float>(StatisticsId::RUN_TIME);
    }
    // setupUvws(*storage);

    callbacks = makeAuto<GuiCallbacks>(*controller);


    //     ArrayView<const Float> omega
}

Size RunPlayer::getFileCount(const Path& pathMask) const {
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
            AutoPtr<IOutput> io = getOutput(path);
            const Outcome result = io->load(path, *storage, stats);
            if (!result) {
                executeOnMainThread([path] {
                    wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                });
                break;
            }
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
    callbacks->onRunEnd(*storage, stats);
    this->tearDownInternal(stats);
}

void RunPlayer::tearDown(const Statistics& UNUSED(stats)) {}

class PlayerPlugin : public IPluginControls {
public:
    virtual void create(wxWindow* parent, wxSizer* sizer) override {
        wxSlider* slider = new wxSlider(parent, wxID_ANY, 0, 0, 100);
        slider->SetSize(wxSize(800, 100));
        sizer->Add(slider);
        wxStaticText* text = new wxStaticText(parent, wxID_ANY, "TEST TEST");
        sizer->Add(text);
    }

    virtual void statusChanges(const RunStatus UNUSED(newStatus)) override {}
};

bool App::OnInit() {
    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    Path fileMask;
    if (wxTheApp->argc == 1) {
        const std::string desc = "SPH state files (*.ssf)|*.ssf|Pkdgrav output files (*.bt)|*bt|All files|*";
        wxFileDialog dialog(nullptr, "Open file", "", "", desc, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() == wxID_CANCEL) {
            return false;
        }
        std::string s(dialog.GetPath());
        fileMask = Path(s);

    } else {
        fileMask = Path(std::string(wxTheApp->argv[1]));
    }

    GuiSettings gui;
    gui.set(GuiSettingsId::ORTHO_FOV, 1.e6_f)
        .set(GuiSettingsId::ORTHO_VIEW_CENTER, 0.5_f * Vector(1024, 768, 0))
        .set(GuiSettingsId::VIEW_WIDTH, 1024)
        .set(GuiSettingsId::VIEW_HEIGHT, 768)
        .set(GuiSettingsId::IMAGES_WIDTH, 1024)
        .set(GuiSettingsId::IMAGES_HEIGHT, 768)
        .set(GuiSettingsId::WINDOW_WIDTH, 1334)
        .set(GuiSettingsId::WINDOW_HEIGHT, 768)
        .set(GuiSettingsId::PARTICLE_RADIUS, 0.25_f)
        .set(GuiSettingsId::SURFACE_LEVEL, 0.1_f)
        .set(GuiSettingsId::SURFACE_SUN_POSITION, getNormalized(Vector(-0.2f, -0.1f, 1.1f)))
        .set(GuiSettingsId::SURFACE_RESOLUTION, 2.e3_f)
        .set(GuiSettingsId::CAMERA, CameraEnum::ORTHO)
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(0._f, 0._f, -1.e4_f))
        .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
        .set(GuiSettingsId::ORTHO_ZOFFSET, -1.e4_f)
        .set(GuiSettingsId::VIEW_GRID_SIZE, 0._f)
        .set(GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY, std::string(""))
        .set(GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY, std::string(""))
        .set(GuiSettingsId::IMAGES_SAVE, true)
        .set(GuiSettingsId::IMAGES_NAME, std::string("frag_%e_%d.png"))
        .set(GuiSettingsId::IMAGES_MOVIE_NAME, std::string("frag_%e.avi"))
        .set(GuiSettingsId::IMAGES_TIMESTEP, 10._f)
        .set(GuiSettingsId::PALETTE_STRESS, Interval(1.e5_f, 3.e6_f))
        .set(GuiSettingsId::PALETTE_VELOCITY, Interval(0.01_f, 1.e2_f))
        .set(GuiSettingsId::PALETTE_PRESSURE, Interval(-5.e4_f, 5.e4_f))
        .set(GuiSettingsId::PALETTE_ENERGY, Interval(0._f, 1.e3_f))
        .set(GuiSettingsId::PALETTE_RADIUS, Interval(700._f, 3.e3_f))
        .set(GuiSettingsId::PALETTE_GRADV, Interval(0._f, 1.e-5_f))
        .set(GuiSettingsId::PLOT_INTEGRALS, PlotEnum::ALL);

    if (fileMask.native().substr(fileMask.native().size() - 3) == ".bt") {
        // pkdgrav file, override some options
        gui.set(GuiSettingsId::ORTHO_FOV, 1.e6_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::PARTICLE_RADIUS, 1._f);
    }

    /*if (FileSystem::pathExists(Path("gui.sph"))) {
        gui.loadFromFile(Path("gui.sph"));
    } else {
        gui.saveToFile(Path("gui.sph"));
    }*/

    controller = makeAuto<Controller>(gui, makeAuto<PlayerPlugin>());

    AutoPtr<RunPlayer> run = makeAuto<RunPlayer>(fileMask);
    run->setController(controller.get());
    controller->start(std::move(run));
    return true;
}

NAMESPACE_SPH_END
