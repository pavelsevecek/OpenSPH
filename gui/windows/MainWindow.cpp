#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/CameraJobs.h"
#include "gui/windows/GridPage.h"
#include "gui/windows/GuiSettingsDialog.h"
#include "gui/windows/NodePage.h"
#include "gui/windows/PlotView.h"
#include "gui/windows/RenderPage.h"
#include "gui/windows/RunPage.h"
#include "gui/windows/SessionDialog.h"
#include "io/FileSystem.h"
#include "objects/utility/IteratorAdapters.h"
#include "post/Plot.h"
#include "run/jobs/GeometryJobs.h"
#include "run/jobs/IoJobs.h"
#include "run/jobs/ParticleJobs.h"
#include <fstream>
#include <wx/aboutdlg.h>
#include <wx/aui/auibook.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>

NAMESPACE_SPH_BEGIN

constexpr int NOTEBOOK_ID = 4257;

wxAuiNotebook* findNotebook() {
    return dynamic_cast<wxAuiNotebook*>(wxWindow::FindWindowById(NOTEBOOK_ID));
}


static Expected<Path> getRecentSessionCache() {
    Expected<Path> userData = FileSystem::getUserDataDirectory();
    SPH_ASSERT(userData);
    if (userData) {
        return userData.value() / Path("opensph/recent.csv");
    } else {
        return userData;
    }
}

static Array<Path> getRecentSessions() {
    if (Expected<Path> recentCache = getRecentSessionCache()) {
        try {
            std::ifstream ifs(recentCache->native());
            std::string line;
            if (std::getline(ifs, line)) {
                Array<std::string> strings = split(line, ',');
                Array<Path> paths;
                for (std::string& s : strings) {
                    paths.emplaceBack(s);
                }
                return paths;
            }
        } catch (const std::exception& UNUSED(e)) {
            // do nothing
        }
    }
    return {};
}

constexpr Size MAX_CACHE_SIZE = 8;

static void addToRecentSessions(const Path& sessionPath) {
    SPH_ASSERT(!sessionPath.empty());
    Array<Path> sessions = getRecentSessions();
    auto sessionIter = std::find(sessions.begin(), sessions.end(), sessionPath);
    if (sessionIter != sessions.end()) {
        // already in the list, remove to move it to the top
        sessions.remove(sessionIter - sessions.begin());
    }
    sessions.insert(0, sessionPath);
    if (sessions.size() > MAX_CACHE_SIZE) {
        sessions.pop();
    }

    if (Expected<Path> recentCache = getRecentSessionCache()) {
        try {
            FileSystem::createDirectory(recentCache->parentPath());
            std::ofstream ofs(recentCache->native());
            for (Size i = 0; i < sessions.size(); ++i) {
                ofs << sessions[i].native();
                if (i != sessions.size() - 1) {
                    ofs << ",";
                }
            }
        } catch (const std::exception& UNUSED(e)) {
        }
    }
}

class NodeManagerCallbacks : public INodeManagerCallbacks {
private:
    MainWindow* window;

public:
    NodeManagerCallbacks(MainWindow* window)
        : window(window) {}

    virtual void startRun(SharedPtr<INode> node,
        const RunSettings& globals,
        const std::string& name) const override {
        window->addRunPage(std::move(node), globals, name);
    }

    virtual void startRender(SharedPtr<INode> node,
        const RunSettings& globals,
        const std::string& name) const override {
        window->addRenderPage(std::move(node), globals, name);
    }

    virtual void markUnsaved(bool UNUSED(addToUndo)) const override {
        window->markSaved(false);
    }
};


MainWindow::MainWindow(const Path& openPath)
    : wxFrame(nullptr,
          wxID_ANY,
#ifdef SPH_DEBUG
          std::string("OpenSPH - build: ") + __DATE__ + " (DEBUG)",
#else
          std::string("OpenSPH - build: ") + __DATE__,
#endif
          wxDefaultPosition,
          wxSize(1024, 768)) {

    this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainWindow::onClose));

    this->Maximize();
    this->SetAutoLayout(true);

    // close button does not work in wxGTK
    notebook = new wxAuiNotebook(this,
        NOTEBOOK_ID,
        wxDefaultPosition,
        wxDefaultSize,
        wxAUI_NB_DEFAULT_STYLE & ~wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
    notebook->SetMinSize(wxSize(1024, 768));
    notebook->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& evt) {
        const int code = evt.GetKeyCode();
        if (evt.ControlDown() && code >= int('1') && code <= int('9')) {
            notebook->SetSelection(code - int('1'));
        }
        evt.Skip();
    });

    nodePage = new NodeWindow(notebook, makeShared<NodeManagerCallbacks>(this), Project::getInstance());
    notebook->AddPage(nodePage, "Unnamed session");

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(notebook, 1, wxALL | wxEXPAND);

    this->SetSizer(sizer);

    wxMenuBar* bar = new wxMenuBar();

    wxMenu* projectMenu = this->createProjectMenu();
    bar->Append(projectMenu, "&Project");

    runMenu = this->createRunMenu();
    bar->Append(runMenu, "&Simulation");

    wxMenu* analysisMenu = this->createAnalysisMenu();
    bar->Append(analysisMenu, "&Analysis");

    wxMenu* resultMenu = this->createResultMenu();
    bar->Append(resultMenu, "&Result");

    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(NodeWindow::ID_PROPERTIES, "&Node properties");
    viewMenu->Append(NodeWindow::ID_LIST, "&Node list");
    viewMenu->Append(NodeWindow::ID_PALETTE, "&Palette setup");
    viewMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        nodePage->showPanel(NodeWindow::PanelId(evt.GetId()));
    });
    bar->Append(viewMenu, "&Window");

    wxMenu* helpMenu = new wxMenu();
    bar->Append(helpMenu, "&Help");
    helpMenu->Append(6000, "&About");
    helpMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [](wxCommandEvent& evt) { //
        switch (evt.GetId()) {
        case 0: {
            wxAboutDialogInfo info;
            info.SetName("OpenSPH");
#ifdef SPH_VERSION
            info.SetVersion(SPH_STR(SPH_VERSION));
#else
            info.SetVersion("unknown");
#endif

            std::string desc;
#ifdef SPH_DEBUG
            desc += "Debug build\n";
#else
            desc += "Release build\n";
#endif
#ifdef SPH_PROFILE
            desc += "Profiling enabled\n";
#endif
#ifdef SPH_USE_TBB
            desc += "Parallelization: TBB\n";
#elif SPH_USE_OPENMP
            desc += "Parallelization: OpenMP\n";
#else
            desc += "Parallelization: built-in thread pool\n";
#endif
#ifdef SPH_USE_EIGEN
            desc += "Eigen: enabled\n";
#else
            desc += "Eigen: disabled\n";
#endif
#ifdef SPH_USE_VDB
            desc += "OpenVDB: enabled\n";
#else
            desc += "OpenVDB: disabled\n";
#endif
#ifdef SPH_USE_CHAISCRIPT
            desc += "Chaiscript: enabled";
#else
            desc += "Chaiscript: disabled";
#endif
            info.SetDescription(desc);
            info.SetCopyright("Pavel Sevecek <sevecek@sirrah.troja.mff.cuni.cz>");

            wxAboutBox(info);
            break;
        }
        default:
            NOT_IMPLEMENTED;
        }
    });

    this->SetMenuBar(bar);
    this->enableMenus(0);

    notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, [this](wxAuiNotebookEvent& evt) {
        const int pageId = evt.GetSelection();
        wxWindow* page = notebook->GetPage(pageId);
        ClosablePage* closablePage = dynamic_cast<RunPage*>(page);
        if (!closablePage || !this->closePage(closablePage)) {
            evt.Veto();
        }
    });

    notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, [this](wxAuiNotebookEvent& evt) {
        const int pageId = evt.GetSelection();
        this->enableMenus(pageId);
    });


    if (!openPath.empty()) {
        /// \todo generalize
        const std::string ext = openPath.extension().native();
        if (getIoEnum(ext)) {
            this->open(openPath, true);
        } else if (ext == "sph") {
            this->load(openPath);
        } else {
            wxMessageBox("Unrecognized file format", "Error", wxOK);
        }
    }

    Project& project = Project::getInstance();
    project.getGuiSettings().accessor = [this](GuiSettingsId UNUSED(id)) { markSaved(false); };
}

void MainWindow::saveAs() {
    Optional<Path> selectedPath = doSaveFileDialog("Save session", { { "OpenSPH session", "sph" } });
    if (selectedPath) {
        this->setProjectPath(selectedPath.value());
        this->save();
    }
}

void MainWindow::save() {
    SPH_ASSERT(!projectPath.empty());
    BusyCursor wait(this);

    Config config;
    // get project data (gui, palettes, ...)
    Project& project = Project::getInstance();
    project.save(config);

    // get node data
    nodePage->save(config);

    config.save(projectPath);

    this->markSaved(true);

    Expected<Path> absolutePath = FileSystem::getAbsolutePath(projectPath);
    SPH_ASSERT(absolutePath);
    if (absolutePath) {
        addToRecentSessions(absolutePath.value());
    }
}

template <typename TInput>
bool isSph(const Path& path) {
    TInput input;
    Expected<typename TInput::Info> info = input.getInfo(path);
    return info && info->runType == RunTypeEnum::SPH;
}

void MainWindow::open(const Path& openPath, const bool setDefaults) {
    BusyCursor wait(this);

    if (setDefaults) {
        // if loading a file specified as parameter, modify defaults if its SPH
        const bool isSphSim = isSph<BinaryInput>(openPath) || isSph<CompressedInput>(openPath);
        const bool isMiluphSim = openPath.extension() == Path("h5");
        if (isSphSim || isMiluphSim) {
            Project::getInstance().getGuiSettings().set(GuiSettingsId::PARTICLE_RADIUS, 0.35_f);
        }
    }
    AutoPtr<Controller> controller = makeAuto<Controller>(notebook);
    controller->open(openPath);

    const Size index = notebook->GetPageCount();
    RunPage* page = &*controller->getPage();
    SPH_ASSERT(page != nullptr);

    RunData data;
    data.controller = std::move(controller);
    data.isRun = false;
    runs.insert(page, std::move(data));

    const Path displayedPath = openPath.parentPath().fileName() / openPath.fileName();
    notebook->AddPage(page, displayedPath.native());
    notebook->SetSelection(index);

    this->enableMenus(index);
}

void MainWindow::load(const Path& openPath) {
    BusyCursor wait(this);

    Path pathToLoad;
    if (openPath.empty()) {
        Optional<Path> selectedPath = doOpenFileDialog("Open session", { { "OpenSPH session", "sph" } });
        if (selectedPath && FileSystem::pathExists(selectedPath.value())) {
            pathToLoad = selectedPath.value();
        } else {
            return;
        }
    } else {
        pathToLoad = openPath;
    }

    if (!FileSystem::pathExists(pathToLoad)) {
        wxMessageBox("File '" + pathToLoad.native() + "' does not exist.");
        return;
    }

    const bool removed = this->removeAll();
    if (!removed) {
        return;
    }

    Config config;
    try {
        config.load(pathToLoad);
    } catch (const Exception& e) {
        wxMessageBox(std::string("Cannot load: ") + e.what(), "Error", wxOK);
        return;
    }

    try {
        Project& project = Project::getInstance();
        project.load(config);
        nodePage->load(config);
    } catch (const Exception& e) {
        wxMessageBox(std::string("Cannot load: ") + e.what(), "Error", wxOK);
        return;
    }

    this->setProjectPath(pathToLoad);

    Expected<Path> absolutePath = FileSystem::getAbsolutePath(pathToLoad);
    SPH_ASSERT(absolutePath);
    if (absolutePath) {
        addToRecentSessions(absolutePath.value());
    }
}

void MainWindow::setProjectPath(const Path& newPath) {
    projectPath = newPath;
    const int pageIndex = notebook->GetPageIndex(nodePage);
    if (!projectPath.empty()) {
        notebook->SetPageText(
            pageIndex, "Session '" + projectPath.fileName().removeExtension().native() + "'");
    } else {
        notebook->SetPageText(pageIndex, "Unnamed session");
    }
}

void MainWindow::markSaved(const bool saved) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    if (savedFlag == saved) {
        return;
    }

    savedFlag = saved;
    if (saved) {
        this->setProjectPath(projectPath); // remove the '*' mark
    } else {
        const int pageIndex = notebook->GetPageIndex(nodePage);
        notebook->SetPageText(pageIndex, notebook->GetPageText(pageIndex) + " *");
    }
}

enum ProjectMenuId {
    PR_NEW_SESSION = 1000,
    PR_SAVE_SESSION,
    PR_SAVE_SESSION_AS,
    PR_OPEN_SESSION,
    PR_VISUALIZATION,
    PR_SHARED_PROPERTIES,
    PR_BATCH_SETUP,
    PR_QUIT,
};

wxMenu* MainWindow::createProjectMenu() {
    wxMenu* projectMenu = new wxMenu();
    projectMenu->Append(PR_NEW_SESSION, "&New session\tCtrl+N");
    projectMenu->Append(PR_SAVE_SESSION, "&Save session\tCtrl+S");
    projectMenu->Append(PR_SAVE_SESSION_AS, "&Save session as");
    projectMenu->Append(PR_OPEN_SESSION, "&Open session\tCtrl+Shift+O");

    wxMenu* recentMenu = new wxMenu();
    projectMenu->AppendSubMenu(recentMenu, "&Recent");
    projectMenu->Append(PR_VISUALIZATION, "&Visualization settings...");
    projectMenu->Append(PR_SHARED_PROPERTIES, "&Shared properties...");
    projectMenu->Append(PR_BATCH_SETUP, "&Batch setup...\tCtrl+B");
    projectMenu->Append(PR_QUIT, "&Quit");

    SharedPtr<Array<Path>> recentSessions = makeShared<Array<Path>>();
    *recentSessions = getRecentSessions();
    for (Size i = 0; i < recentSessions->size(); ++i) {
        recentMenu->Append(i, (*recentSessions)[i].native());
    }

    // wx handlers need to be copyable, we thus cannot capture Array
    recentMenu->Bind(wxEVT_COMMAND_MENU_SELECTED,
        [this, recentSessions](wxCommandEvent& evt) { this->load((*recentSessions)[evt.GetId()]); });

    projectMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        switch (evt.GetId()) {
        case PR_NEW_SESSION: {
            // end running simulations
            if (!this->removeAll()) {
                break;
            }
            // ask user if unsaved
            if (checkUnsavedSession() == wxCANCEL) {
                break;
            }
            auto nameMgr = nodePage->makeUniqueNameManager();
            SessionDialog* dialog = new SessionDialog(this, nameMgr);
            if (dialog->ShowModal() == wxID_OK) {
                this->setProjectPath(Path());
                nodePage->reset();
                auto node = dialog->selectedPreset();
                if (node) {
                    nodePage->addNodes(*node);
                }
            }
            dialog->Destroy();
            break;
        }
        case PR_SAVE_SESSION: {
            if (projectPath.empty()) {
                this->saveAs();
            } else {
                this->save();
            }
            break;
        }
        case PR_SAVE_SESSION_AS:
            this->saveAs();
            break;
        case PR_OPEN_SESSION:
            this->load();
            break;
        case PR_VISUALIZATION: {
            GuiSettingsDialog* dialog = new GuiSettingsDialog(this);
            dialog->ShowModal();
            break;
        }
        case PR_SHARED_PROPERTIES:
            notebook->SetSelection(0);
            nodePage->showGlobals();
            break;
        case PR_BATCH_SETUP:
            nodePage->showBatchDialog();
            break;
        case PR_QUIT:
            this->Close();
            break;
        default:
            NOT_IMPLEMENTED;
        }
    });
    return projectMenu;
}

enum ResultsMenuId {
    RE_OPEN = 2000,
    RE_CLOSE,
};

wxMenu* MainWindow::createResultMenu() {
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(RE_OPEN, "&Open\tCtrl+O");
    fileMenu->Append(RE_CLOSE, "&Close current\tCtrl+W");

    fileMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        switch (evt.GetId()) {
        case RE_OPEN: {
            Optional<Path> path = doOpenFileDialog("Open file",
                { { "SPH state file", "ssf" },
                    { "SPH data file", "sdf" },
                    { "miluphcuda output files", "h5" },
                    { "Text .tab files", "tab" } });
            if (path) {
                this->open(path.value(), false);
            }
            break;
        }
        case RE_CLOSE: {
            wxWindow* page = notebook->GetCurrentPage();
            ClosablePage* closablePage = dynamic_cast<ClosablePage*>(page);
            if (!closablePage) {
                // cannot close this page
                break;
            }

            closePage(closablePage);
            break;
        }
        default:
            NOT_IMPLEMENTED
        }

    });
    return fileMenu;
}

enum RunMenuId {
    RU_START = 3000,
    RU_START_BATCH,
    RU_START_SCRIPT,
    RU_RESTART,
    RU_PAUSE,
    RU_STOP,
    RU_SAVE_STATE,
    RU_CREATE_CAMERA,
    RU_CLOSE_ALL,
};

wxMenu* MainWindow::createRunMenu() {
    wxMenu* runMenu = new wxMenu();
    runMenu->Append(RU_START, "S&tart run\tCtrl+R");
    runMenu->Append(RU_START_BATCH, "Start batch");
    runMenu->Append(RU_START_SCRIPT, "Start script");
    runMenu->Append(RU_RESTART, "&Restart");
    runMenu->Append(RU_PAUSE, "&Pause");
    runMenu->Append(RU_STOP, "St&op");
    runMenu->Append(RU_SAVE_STATE, "&Save current state");
    runMenu->Append(RU_CREATE_CAMERA, "Make camera node");
    runMenu->Append(RU_CLOSE_ALL, "Close all");

    runMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this, runMenu](wxCommandEvent& evt) { //
        // options not related to a particular controller
        if (evt.GetId() == RU_START) {
            nodePage->selectRun();
            return;
        } else if (evt.GetId() == RU_START_SCRIPT) {
#ifdef SPH_USE_CHAISCRIPT
            Optional<Path> scriptPath = doOpenFileDialog("Chai script", { { "Chai script", "chai" } });
            if (scriptPath) {
                nodePage->startScript(scriptPath.value());
            }
#else
            wxMessageBox("The code needs to be compiled with ChaiScript support.", "No ChaiScript", wxOK);
#endif
            return;
        }

        RunPage* page = dynamic_cast<RunPage*>(notebook->GetCurrentPage());
        if (!page) {
            return;
        }
        RawPtr<Controller> controller = runs[page].controller.get();

        switch (evt.GetId()) {
        case RU_RESTART:
            controller->stop(true);
            controller->restart();
            break;
        case RU_PAUSE: {
            RunStatus status = controller->getStatus();
            wxMenuItem* item = runMenu->FindItem(RU_PAUSE);
            if (status == RunStatus::PAUSED) {
                controller->restart();
                item->SetItemLabel("&Pause");
            } else {
                controller->pause();
                item->SetItemLabel("Un&pause");
            }
            break;
        }
        case RU_STOP:
            controller->stop();
            break;
        case RU_SAVE_STATE: {
            Optional<Path> path = doSaveFileDialog("Save state file", getOutputFormats());
            if (!path) {
                return;
            }
            controller->saveState(path.value());
            break;
        }
        case RU_CREATE_CAMERA: {
            AutoPtr<ICamera> camera = controller->getCurrentCamera();
            auto nameMgr = nodePage->makeUniqueNameManager();
            AutoPtr<OrthoCameraJob> job = makeAuto<OrthoCameraJob>(nameMgr.getName("hand-held camera"));
            VirtualSettings settings = job->getSettings();
            AffineMatrix frame = camera->getFrame();
            const Vector posKm = frame.translation() * 1.e-3_f;

            settings.set(GuiSettingsId::CAMERA_POSITION, posKm);
            settings.set(GuiSettingsId::CAMERA_UP, frame.row(1));
            settings.set(GuiSettingsId::CAMERA_TARGET, posKm + frame.row(2));
            const Optional<float> wtp = camera->getWorldToPixel();
            if (wtp) {
                settings.set(GuiSettingsId::CAMERA_ORTHO_FOV, 1.e-3_f * camera->getSize().y / wtp.value());
            }
            nodePage->createNode(std::move(job));
            notebook->SetSelection(notebook->GetPageIndex(nodePage));
            break;
        }
        case RU_CLOSE_ALL:
            this->removeAll();
            break;
        default:
            NOT_IMPLEMENTED;
        }
    });

    return runMenu;
}

enum AnalysisMenuId {
    AN_CURRENT_SFD = 4000,
    AN_PREDICTED_SFD,
    AN_VELOCITY_HIST,
    AN_DENSITY,
    AN_ENERGY,
    AN_PRESSURE,
    AN_FRAGMENTS,
};


wxMenu* MainWindow::createAnalysisMenu() {
    wxMenu* analysisMenu = new wxMenu();
    analysisMenu->Append(AN_CURRENT_SFD, "Current SFD");
    analysisMenu->Append(AN_PREDICTED_SFD, "Predicted SFD");
    analysisMenu->Append(AN_VELOCITY_HIST, "Velocity histogram");
    analysisMenu->Append(AN_DENSITY, "Density profile");
    analysisMenu->Append(AN_ENERGY, "Energy profile");
    analysisMenu->Append(AN_PRESSURE, "Pressure profile");
    analysisMenu->Append(AN_FRAGMENTS, "Fragment parameters");
    analysisMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        BusyCursor wait(this);
        RunPage* page = dynamic_cast<RunPage*>(notebook->GetCurrentPage());
        if (!page) {
            return;
        }
        RawPtr<Controller> controller = runs[page].controller.get();

        if (evt.GetId() == AN_FRAGMENTS) { // not a plot, requires special handling
            GridPage* gridPage = new GridPage(notebook, wxSize(800, 600), controller->getStorage());

            const Size index = notebook->GetPageCount();
            notebook->AddPage(gridPage, "Fragments");
            notebook->SetSelection(index);
            return;
        }

        // plot options below
        LockingPtr<IPlot> plot;
        switch (evt.GetId()) {
        case AN_CURRENT_SFD:
        case AN_PREDICTED_SFD: {
            Post::ComponentFlag flag =
                evt.GetId() == 0 ? Post::ComponentFlag::OVERLAP : Post::ComponentFlag::ESCAPE_VELOCITY;

            Array<AutoPtr<IPlot>> multiplot;
            multiplot.emplaceBack(makeAuto<SfdPlot>(flag, 0._f));
            Project& project = Project::getInstance();
            const std::string overplotSfd =
                project.getGuiSettings().get<std::string>(GuiSettingsId::PLOT_OVERPLOT_SFD);
            if (!overplotSfd.empty()) {
                multiplot.emplaceBack(getDataPlot(Path(overplotSfd), "overplot"));
            }
            plot = makeLocking<MultiPlot>(std::move(multiplot));
            break;
        }
        case AN_VELOCITY_HIST:
            plot = makeLocking<HistogramPlot>(Post::HistogramId::VELOCITIES, NOTHING, 0._f, "Velocity");
            break;
        case AN_DENSITY:
            plot = makeLocking<RadialDistributionPlot>(QuantityId::DENSITY);
            break;
        case AN_ENERGY:
            plot = makeLocking<RadialDistributionPlot>(QuantityId::ENERGY);
            break;
        case AN_PRESSURE:
            if (!controller->getStorage().has(QuantityId::PRESSURE)) {
                wxMessageBox("No pressure data", "Error", wxOK);
                return;
            }
            plot = makeLocking<RadialDistributionPlot>(QuantityId::PRESSURE);
            break;
        default:
            NOT_IMPLEMENTED;
        }
        Statistics stats;
        stats.set(StatisticsId::RUN_TIME, 0._f);
        plot->onTimeStep(controller->getStorage(), stats);

        PlotPage* plotPage = new PlotPage(notebook, wxSize(800, 600), wxSize(25, 25), plot);

        const Size index = notebook->GetPageCount();
        // needs to be called before, AddPage calls onPaint, which locks the mutex
        const std::string caption = plot->getCaption();
        notebook->AddPage(plotPage, caption);
        notebook->SetSelection(index);

    });
    return analysisMenu;
}

void MainWindow::addRunPage(SharedPtr<INode> node, const RunSettings& globals, const std::string pageName) {
    AutoPtr<Controller> controller = makeAuto<Controller>(notebook);
    controller->start(std::move(node), globals);

    RunPage* page = &*controller->getPage();
    RunData data;
    data.controller = std::move(controller);
    data.isRun = true;
    runs.insert(page, std::move(data));

    const Size index = notebook->GetPageCount();
    notebook->AddPage(page, pageName);
    notebook->SetSelection(index);

    this->enableMenus(index);
}

void MainWindow::addRenderPage(SharedPtr<INode> node,
    const RunSettings& globals,
    const std::string pageName) {
    RenderPage* page = new RenderPage(notebook, globals, node);

    const Size index = notebook->GetPageCount();
    notebook->AddPage(page, pageName);
    notebook->SetSelection(index);

    this->enableMenus(index);
}

bool MainWindow::removeAll() {
    for (int i = notebook->GetPageCount() - 1; i >= 0; --i) {
        ClosablePage* closablePage = dynamic_cast<ClosablePage*>(notebook->GetPage(i));
        if (closablePage && !this->closePage(closablePage)) {
            return false;
        }
    }
    return true;
}

void MainWindow::onClose(wxCloseEvent& evt) {
    if (checkUnsavedSession() == wxCANCEL) {
        evt.Veto();
        return;
    }
    this->Destroy();
}

void MainWindow::enableMenus(const Size id) {
    /// \todo avoid hardcoded indices

    wxMenuBar* bar = this->GetMenuBar();
    RunPage* page = dynamic_cast<RunPage*>(notebook->GetPage(id));
    if (page == nullptr) {
        enableRunMenu(false, false);
        // disable analysis
        bar->EnableTop(2, false);
        return;
    }
    SPH_ASSERT(runs.contains(page));

    const bool enableControls = runs[page].isRun;
    enableRunMenu(enableControls, true);
    // enable/disable analysis
    bar->EnableTop(2, true);
}

void MainWindow::enableRunMenu(const bool enableControls, const bool enableCamera) {
    wxMenuItemList& list = runMenu->GetMenuItems();
    for (Size i = 0; i < list.size(); ++i) {
        const int menuIdx = i + RU_START; // start has to be the first one
        if (menuIdx == RU_START || menuIdx == RU_START_SCRIPT || menuIdx == RU_START_BATCH) {
            // always enabled
            list[i]->Enable(true);
            continue;
        }
        if (menuIdx == RU_CREATE_CAMERA) {
            list[i]->Enable(enableCamera);
        } else {
            list[i]->Enable(enableControls);
        }
    }
}

bool MainWindow::closePage(ClosablePage* page) {
    if (!page->close()) {
        // veto'd
        return false;
    }

    // destroy the associated controller
    if (RunPage* runPage = dynamic_cast<RunPage*>(page)) {
        runs.remove(runPage);
    }
    notebook->DeletePage(notebook->GetPageIndex(page));
    return true;
}

int MainWindow::checkUnsavedSession() {
    if (savedFlag) {
        return true;
    }
    const int retval = wxMessageBox("Save unsaved changes", "Save?", wxYES_NO | wxCANCEL | wxCENTRE);
    if (retval == wxYES) {
        if (projectPath.empty()) {
            this->saveAs();
        } else {
            this->save();
        }
    }
    return retval;
}
NAMESPACE_SPH_END
