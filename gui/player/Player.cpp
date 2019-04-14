#include "gui/player/Player.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Texture.h"
#include "gui/player/TimeLine.h"
#include "gui/windows/SettingsDialog.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "objects/Exceptions.h"
#include "objects/utility/StringUtils.h"
#include "timestepping/ISolver.h"
#include "timestepping/TimeStepping.h"
#include <wx/msgdlg.h>
#include <wx/slider.h>
#include <wx/stattext.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

RunPlayer::RunPlayer(const Path& fileMask, Function<void(int)> onNewFrame)
    : fileMask(fileMask)
    , onNewFrame(onNewFrame) {}

class TabInput : public IInput {
private:
    TextInput input;

public:
    TabInput()
        : input(OutputQuantityFlag::MASS | OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY) {}

    virtual Outcome load(const Path& path, Storage& storage, Statistics& stats) override {
        Outcome result = input.load(path, storage, stats);
        if (!result) {
            return result;
        }

        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            r[i][H] = 1.e-2_f;
        }

        return SUCCESS;
    }
};

/// \todo possibly move to Factory
static AutoPtr<IInput> getInput(const Path& path) {
    if (path.extension() == Path("ssf")) {
        return makeAuto<BinaryInput>();
    } else if (path.extension() == Path("scf")) {
        return makeAuto<CompressedInput>();
    } else if (path.extension() == Path("tab")) {
        return makeAuto<TabInput>();
    } else {
        std::string str = path.native();
        if (str.substr(str.size() - 3) == ".bt") {
            return makeAuto<PkdgravInput>();
        }
    }
    throw InvalidSetup("Unknown file type: " + path.native());
}

void RunPlayer::setUp() {
    logger = makeAuto<StdOutLogger>();

    fileMap.clear();
    OutputFile of(fileMask);
    if (of.hasWildcard()) {
        //  frame sequence
        fileMap = getSequenceFiles(fileMask);
        if (fileMap.size() > 1) {
            logger->write("Loading sequence of ", fileMap.size(), " files");
        }
    } else {
        // single frame
        const Optional<Size> frame = OutputFile::getDumpIdx(fileMask);
        fileMap.insert(std::make_pair(frame.valueOr(0), fileMask));
    }

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    const Path firstPath = of.hasWildcard() ? fileMap.begin()->second : fileMask;
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
    logger->write(
        "Loaded file ", firstPath.fileName().native(), " with ", storage->getParticleCnt(), " particles");

    // setupUvws(*storage);

    callbacks = makeAuto<GuiCallbacks>(*controller);

    // dummy solver, used to avoid calling create on stuff
    struct PlayerSolver : public ISolver {
        virtual void integrate(Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

        virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
    };
    solver = makeAuto<PlayerSolver>();

    // dummy timestepping, otherwise we would erase highest derivatives
    struct PlayerTimestepping : public ITimeStepping {
        using ITimeStepping::ITimeStepping;

        virtual void stepImpl(IScheduler& UNUSED(scheduler),
            ISolver& UNUSED(solver),
            Statistics& UNUSED(stats)) override {}
    };
    timeStepping = makeAuto<PlayerTimestepping>(storage, settings);
}

void RunPlayer::run() {
    ASSERT(storage);
    setNullToDefaults();
    logger->write("Running:");

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, loadedTime);
    callbacks->onRunStart(*storage, stats);

    const Size stepMilliseconds = Size(1000._f / fps);
    Size i = 0;
    for (auto frameAndPath : fileMap) {
        Timer stepTimer;
        stats.set(StatisticsId::RELATIVE_PROGRESS, Float(i) / fileMap.size());
        callbacks->onTimeStep(*storage, stats);
        if (onNewFrame) {
            onNewFrame(frameAndPath.first);
        }

        if (i != fileMap.size() - 1) {
            storage = makeShared<Storage>();
            const Path path = frameAndPath.second;
            Timer loadTimer;
            AutoPtr<IInput> io = getInput(path);
            const Outcome result = io->load(path, *storage, stats);
            if (!result) {
                executeOnMainThread([path] {
                    wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                });
            }
            logger->write("Loaded ",
                path.fileName().native(),
                " in ",
                loadTimer.elapsed(TimerUnit::MILLISECOND),
                " ms");
            // setupUvws(*storage);

            const Size elapsed = stepTimer.elapsed(TimerUnit::MILLISECOND);
            if (elapsed < stepMilliseconds) {
                std::this_thread::sleep_for(std::chrono::milliseconds(stepMilliseconds - elapsed));
            }
        }

        ++i;
        if (callbacks->shouldAbortRun()) {
            break;
        }
    }
    if (fileMap.size() > 1) {
        logger->write("File sequence finished");
    } else {
        logger->write("File finished");
    }
    this->tearDownInternal(stats);
}

void RunPlayer::tearDown(const Statistics& UNUSED(stats)) {}

bool App::OnInit() {
    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    Path fileMask;
    if (wxTheApp->argc == 1) {
        Optional<Path> selectedFile = doOpenFileDialog("Open file",
            {
                { "SPH state files", "ssf" },
                { "SPH compressed files", "scf" },
                { "Pkdgrav output files", "bt" },
                { "Text .tab files", "tab" },
            });
        if (!selectedFile) {
            return false;
        }
        fileMask = selectedFile.value();

    } else {
        fileMask = Path(std::string(wxTheApp->argv[1]));
        fileMask = FileSystem::getAbsolutePath(fileMask);
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
        .set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(0._f))
        .set(GuiSettingsId::PERSPECTIVE_POSITION, Vector(-20, -19, 40))
        //.set(GuiSettingsId::PERSPECTIVE_TARGET, Vector(0._f, -1.e6_f, 0._f))
        .set(GuiSettingsId::BACKGROUND_COLOR, Rgba::black())
        .set(GuiSettingsId::PERSPECTIVE_CLIP_NEAR, EPS)
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::ORTHO_CUTOFF, 0._f) // 10000._f)
        .set(GuiSettingsId::ORTHO_ZOFFSET, -1.e6_f)
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
        .set(GuiSettingsId::PLOT_INTEGRALS, EMPTY_FLAGS)
        /*PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_MOMENTUM |
            PlotEnum::TOTAL_ANGULAR_MOMENTUM*/
        .set(GuiSettingsId::PLOT_OVERPLOT_SFD, std::string("/home/pavel/Dropbox/family.dat_hc"));

    if (runType == RunTypeEnum::NBODY) {
        gui.set(GuiSettingsId::PLOT_INTEGRALS,
            PlotEnum::KINETIC_ENERGY | PlotEnum::INTERNAL_ENERGY | PlotEnum::TOTAL_ENERGY |
                PlotEnum::TOTAL_MOMENTUM | PlotEnum::TOTAL_ANGULAR_MOMENTUM | PlotEnum::PARTICLE_SFD);
    }

    if (fileMask.native().substr(fileMask.native().size() - 3) == ".bt") {
        // pkdgrav file, override some options
        gui.set(GuiSettingsId::ORTHO_FOV, 1.e6_f)
            .set(GuiSettingsId::ORTHO_CUTOFF, 0._f)
            .set(GuiSettingsId::PARTICLE_RADIUS, 1._f);
    }

    /// \todo this is ridiculous, but it will do for now
    auto callback = [this](const Path& newPath) {
        AutoPtr<RunPlayer> newRun = makeAuto<RunPlayer>(newPath, nullptr);
        newRun->setController(controller.get());
        controller->start(std::move(newRun));
    };

    AutoPtr<TimeLinePlugin> plugin = makeAuto<TimeLinePlugin>(fileMask, callback);
    auto onNewFrame = [plugin = plugin.get()](int newFrame) { plugin->setFrame(newFrame); };
    controller = makeAuto<Controller>(gui, std::move(plugin));

    AutoPtr<RunPlayer> run = makeAuto<RunPlayer>(fileMask, onNewFrame);
    run->setController(controller.get());
    controller->start(std::move(run));

    return true;
}

NAMESPACE_SPH_END
