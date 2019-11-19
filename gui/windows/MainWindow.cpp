#include "gui/windows/MainWindow.h"
#include "gui/Controller.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/windows/GridPage.h"
#include "gui/windows/NodePage.h"
#include "gui/windows/PlotView.h"
#include "gui/windows/RunPage.h"
#include "io/FileSystem.h"
#include "objects/utility/IteratorAdapters.h"
#include "post/Plot.h"
#include "run/workers/GeometryWorkers.h"
#include "run/workers/IoWorkers.h"
#include "run/workers/ParticleWorkers.h"
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
    Expected<Path> home = FileSystem::getHomeDirectory();
    if (home) {
        return home.value() / Path(".config/opensph/recent.csv");
    } else {
        return home;
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
        } catch (std::exception& UNUSED(e)) {
            // do nothing
        }
    }
    return {};
}

constexpr Size MAX_CACHE_SIZE = 8;

static void addToRecentSessions(const Path& sessionPath) {
    ASSERT(!sessionPath.empty());
    Array<Path> sessions = getRecentSessions();
    if (std::find(sessions.begin(), sessions.end(), sessionPath) != sessions.end()) {
        // already in the list
        /// \todo move to top?
        return;
    }
    sessions.insert(0, sessionPath);
    if (sessions.size() > MAX_CACHE_SIZE) {
        sessions.pop();
    }

    if (Expected<Path> recentCache = getRecentSessionCache()) {
        try {
            std::ofstream ofs(recentCache->native());
            for (Size i = 0; i < sessions.size(); ++i) {
                ofs << sessions[i].native();
                if (i != sessions.size() - 1) {
                    ofs << ",";
                }
            }
        } catch (std::exception& UNUSED(e)) {
        }
    }
}

class NodeManagerCallbacks : public INodeManagerCallbacks {
private:
    MainWindow* window;

public:
    NodeManagerCallbacks(MainWindow* window)
        : window(window) {}

    virtual void startRun(WorkerNode& node, const RunSettings& globals) const override {
        window->addPage(node.sharedFromThis(), globals, node.instanceName());
    }

    virtual void markUnsaved() const override {
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

    nodePage = new NodeWindow(notebook, makeShared<NodeManagerCallbacks>(this));
    notebook->AddPage(nodePage, "Unnamed session");

    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(notebook, 1, wxALL | wxEXPAND);

    this->SetSizer(sizer);

    wxMenuBar* bar = new wxMenuBar();

    wxMenu* projectMenu = this->createProjectMenu();
    bar->Append(projectMenu, "&Project");

    wxMenu* runMenu = this->createRunMenu();
    bar->Append(runMenu, "&Simulation");
    bar->EnableTop(bar->GetMenuCount() - 1, false);

    wxMenu* analysisMenu = this->createAnalysisMenu();
    bar->Append(analysisMenu, "&Analysis");

    wxMenu* resultMenu = this->createResultMenu();
    bar->Append(resultMenu, "&Result");
    // bar->EnableTop(bar->GetMenuCount() - 1, false);

    wxMenu* viewMenu = new wxMenu();
    viewMenu->Append(NodeWindow::ID_PROPERTIES, "&Node properties");
    viewMenu->Append(NodeWindow::ID_LIST, "&Node list");
    viewMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        nodePage->showPanel(NodeWindow::PanelId(evt.GetId()));
    });
    bar->Append(viewMenu, "&Window");

    wxMenu* helpMenu = new wxMenu();
    bar->Append(helpMenu, "&Help");
    helpMenu->Append(0, "&About");
    helpMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [](wxCommandEvent& evt) { //
        switch (evt.GetId()) {
        case 0: {
            wxAboutDialogInfo info;
            info.SetName("OpenSPH");
            info.SetVersion("0.2.4");

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
            desc += "OpenVDB: disabled\n";
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

    notebook->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, [this](wxAuiNotebookEvent& evt) {
        const int pageId = evt.GetSelection();
        wxWindow* page = notebook->GetPage(pageId);
        RunPage* runPage = dynamic_cast<RunPage*>(page);
        NodeWindow* nodePage = dynamic_cast<NodeWindow*>(page);
        if (nodePage || (runPage && !runPage->close())) {
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
        if (ext == "ssf" || ext == "scf" || ext == "tab") {
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
    BusyCursor wait(this);

    Config config;
    // get project data (gui, palettes, ...)
    Project& project = Project::getInstance();
    project.save(config);

    // get node data
    nodePage->save(config);

    config.save(projectPath);

    this->markSaved(true);
}

void MainWindow::open(const Path& openPath, const bool setDefaults) {
    BusyCursor wait(this);

    if (setDefaults) {
        // if loading a file specified as parameter, modify defaults if its SPH
        /// \todo generalize
        BinaryInput input;
        Expected<BinaryInput::Info> info = input.getInfo(openPath);
        if (info && info->runType == RunTypeEnum::SPH) {
            Project::getInstance().getGuiSettings().set(GuiSettingsId::PARTICLE_RADIUS, 0.35);
        }
    }
    AutoPtr<Controller> controller = makeAuto<Controller>(notebook);
    controller->open(openPath);

    const Size index = notebook->GetPageCount();
    RunPage* page = &*controller->getPage();

    const Path displayedPath = openPath.parentPath().fileName() / openPath.fileName();
    notebook->AddPage(page, displayedPath.native());
    notebook->SetSelection(index);

    RunData data;
    data.controller = std::move(controller);
    data.isRun = false;
    runs.insert(page, std::move(data));

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
    } catch (Exception& e) {
        wxMessageBox(std::string("Cannot load: ") + e.what(), "Error", wxOK);
        return;
    }

    try {
        Project& project = Project::getInstance();
        project.load(config);
        nodePage->load(config);
    } catch (Exception& e) {
        wxMessageBox(std::string("Cannot load: ") + e.what(), "Error", wxOK);
        return;
    }

    this->setProjectPath(pathToLoad);
    addToRecentSessions(FileSystem::getAbsolutePath(pathToLoad));
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

wxMenu* MainWindow::createProjectMenu() {
    wxMenu* projectMenu = new wxMenu();
    projectMenu->Append(0, "&New session\tCtrl+N");
    projectMenu->Append(1, "&Save session\tCtrl+S");
    projectMenu->Append(2, "&Save session as");
    projectMenu->Append(3, "&Open session\tCtrl+O");

    wxMenu* recentMenu = new wxMenu();
    projectMenu->AppendSubMenu(recentMenu, "&Recent");
    projectMenu->Append(4, "&Shared properties");
    projectMenu->Append(5, "&Batch run\tCtrl+B");
    projectMenu->Append(6, "&Undo\tCtrl+Z");
    projectMenu->Append(7, "&Quit");

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
        case 0:
            if (this->removeAll()) {
                this->setProjectPath(Path());
                nodePage->reset();
            }
            break;
        case 1: {
            if (projectPath.empty()) {
                this->saveAs();
            } else {
                this->save();
            }
            break;
        }
        case 2:
            this->saveAs();
            break;
        case 3:
            this->load();
            break;
        case 4:
            notebook->SetSelection(0);
            nodePage->showGlobals();
            break;
        case 5:
            nodePage->showBatchDialog();
            break;
        case 6:
            nodePage->undo();
            break;
        case 7:
            this->Close();
            break;
        default:
            NOT_IMPLEMENTED;
        }
    });
    return projectMenu;
}

wxMenu* MainWindow::createResultMenu() {

    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(0, "&Open\tCtrl+O");
    fileMenu->Append(1, "&Close current\tCtrl+W");

    fileMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        switch (evt.GetId()) {
        case 0: {
            Optional<Path> path = doOpenFileDialog("Open file",
                { { "SPH state file", "ssf" },
                    { "SPH compressed file", "scf" },
                    { "Text .tab files", "tab" } });
            if (path) {
                this->open(path.value(), false);
            }
            break;
        }
        case 1: {
            wxWindow* page = notebook->GetCurrentPage();
            if (dynamic_cast<NodeWindow*>(page)) {
                // node page should not be closeable
                break;
            }

            bool canClose = true;
            if (RunPage* runPage = dynamic_cast<RunPage*>(page)) {
                canClose = runPage->close();
            }

            if (canClose) {
                notebook->DeletePage(notebook->GetPageIndex(page));
            }
            break;
        }
        default:
            NOT_IMPLEMENTED
        }

    });
    return fileMenu;
}


/// \brief Helper object used for drawing multiple plots into the same device.
class MultiPlot : public IPlot {
private:
    Array<AutoPtr<IPlot>> plots;

public:
    explicit MultiPlot(Array<AutoPtr<IPlot>>&& plots)
        : plots(std::move(plots)) {}

    virtual std::string getCaption() const override {
        return plots[0]->getCaption(); /// ??
    }

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override {
        ranges.x = ranges.y = Interval();
        for (auto& plot : plots) {
            plot->onTimeStep(storage, stats);
            ranges.x.extend(plot->rangeX());
            ranges.y.extend(plot->rangeY());
        }
    }

    virtual void clear() override {
        ranges.x = ranges.y = Interval();
        for (auto& plot : plots) {
            plot->clear();
        }
    }

    virtual void plot(IDrawingContext& dc) const override {
        for (auto plotAndIndex : iterateWithIndex(plots)) {
            dc.setStyle(plotAndIndex.index());
            plotAndIndex.value()->plot(dc);
        }
    }
};

static Array<Post::HistPoint> getOverplotSfd(const GuiSettings& gui) {
    const Path overplotPath(gui.get<std::string>(GuiSettingsId::PLOT_OVERPLOT_SFD));
    if (overplotPath.empty() || !FileSystem::pathExists(overplotPath)) {
        return {};
    }
    Array<Post::HistPoint> overplotSfd;
    std::ifstream is(overplotPath.native());
    while (true) {
        float value, count;
        is >> value >> count;
        if (!is) {
            break;
        }
        value *= 0.5 /*D->R*/ * 1000 /*km->m*/; // super-specific, should be generalized somehow
        overplotSfd.emplaceBack(Post::HistPoint{ value, Size(round(count)) });
    };
    return overplotSfd;
}

wxMenu* MainWindow::createRunMenu() {
    wxMenu* runMenu = new wxMenu();
    runMenu->Append(0, "&Restart");
    runMenu->Append(1, "&Pause");
    runMenu->Append(2, "&Stop");
    runMenu->Append(3, "&Save state");
    runMenu->Append(4, "Create &node from state");
    runMenu->Append(5, "&Close current\tCtrl+W");
    runMenu->Append(6, "Close all");

    runMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        RunPage* page = dynamic_cast<RunPage*>(notebook->GetCurrentPage());
        if (!page) {
            return;
        }
        RawPtr<Controller> controller = runs[page].controller.get();

        switch (evt.GetId()) {
        case 0:
            controller->restart();
            break;
        case 1:
            controller->pause();
            break;
        case 2:
            controller->stop();
            break;
        case 3: {
            static Array<FileFormat> fileFormats = {
                { "SPH state file", "ssf" },
                { "SPH compressed file", "scf" },
                { "VTK unstructured grid", "vtu" },
                { "Text file", "txt" },
            };
            Optional<Path> path = doSaveFileDialog("Save state file", fileFormats.clone());
            if (!path) {
                return;
            }
            controller->saveState(path.value());
            break;
        }
        case 4: {
            const Storage& storage = controller->getStorage();
            const std::string text("cached " + notebook->GetPageText(notebook->GetSelection()));
            AutoPtr<CachedParticlesWorker> worker = makeAuto<CachedParticlesWorker>(text, storage);
            nodePage->addNode(std::move(worker));
            notebook->SetSelection(notebook->GetPageIndex(nodePage));
            break;
        }
        case 5:
            if (page->close()) {
                notebook->DeletePage(notebook->GetPageIndex(page));
            }
            break;
        case 6:
            this->removeAll();
            break;
        default:
            NOT_IMPLEMENTED;
        }
    });

    return runMenu;
}


wxMenu* MainWindow::createAnalysisMenu() {
    wxMenu* analysisMenu = new wxMenu();
    analysisMenu->Append(0, "Current SFD");
    analysisMenu->Append(1, "Predicted SFD");
    analysisMenu->Append(2, "Velocity histogram");
    analysisMenu->Append(3, "Density profile");
    analysisMenu->Append(4, "Energy profile");
    analysisMenu->Append(5, "Pressure profile");
    analysisMenu->Append(6, "Fragment parameters");
    analysisMenu->Bind(wxEVT_COMMAND_MENU_SELECTED, [this](wxCommandEvent& evt) { //
        BusyCursor wait(this);
        RunPage* page = dynamic_cast<RunPage*>(notebook->GetCurrentPage());
        if (!page) {
            return;
        }
        RawPtr<Controller> controller = runs[page].controller.get();

        if (evt.GetId() == 6) { // not a plot, requires special handling
            GridPage* gridPage = new GridPage(notebook, wxSize(800, 600), controller->getStorage());

            const Size index = notebook->GetPageCount();
            notebook->AddPage(gridPage, "Fragments");
            notebook->SetSelection(index);
            return;
        }

        // plot options below
        LockingPtr<IPlot> plot;
        switch (evt.GetId()) {
        case 0:
        case 1: {
            Post::ComponentFlag flag =
                evt.GetId() == 0 ? Post::ComponentFlag::OVERLAP : Post::ComponentFlag::ESCAPE_VELOCITY;

            Array<AutoPtr<IPlot>> multiplot;
            multiplot.emplaceBack(makeAuto<SfdPlot>(flag, 0._f));

            Project& project = Project::getInstance();
            Array<Post::HistPoint> overplotSfd = getOverplotSfd(project.getGuiSettings());
            if (!overplotSfd.empty()) {
                multiplot.emplaceBack(
                    makeAuto<DataPlot>(overplotSfd, AxisScaleEnum::LOG_X | AxisScaleEnum::LOG_Y, "Overplot"));
            }
            plot = makeLocking<MultiPlot>(std::move(multiplot));
            break;
        }
        case 2:
            plot = makeLocking<HistogramPlot>(Post::HistogramId::VELOCITIES, NOTHING, "Velocity");
            break;
        case 3:
            plot = makeLocking<RadialDistributionPlot>(QuantityId::DENSITY);
            break;
        case 4:
            plot = makeLocking<RadialDistributionPlot>(QuantityId::ENERGY);
            break;
        case 5:
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

void MainWindow::addPage(SharedPtr<WorkerNode> node, const RunSettings& globals, const std::string pageName) {
    AutoPtr<Controller> controller = makeAuto<Controller>(notebook);
    controller->start(node, globals);

    const Size index = notebook->GetPageCount();
    RunPage* page = &*controller->getPage();
    notebook->AddPage(page, pageName);
    notebook->SetSelection(index);

    RunData data;
    data.controller = std::move(controller);
    data.isRun = true;
    runs.insert(page, std::move(data));

    this->enableMenus(index);
}

bool MainWindow::removeAll() {
    for (int i = notebook->GetPageCount() - 1; i >= 0; --i) {
        if (RunPage* page = dynamic_cast<RunPage*>(notebook->GetPage(i))) {
            const bool canClose = page->close();
            if (canClose) {
                notebook->DeletePage(i);
            } else {
                return false;
            }
        }
    }
    return true;
}

void MainWindow::onClose(wxCloseEvent& evt) {
    for (Size i = 0; i < notebook->GetPageCount(); ++i) {
        RunPage* page = dynamic_cast<RunPage*>(notebook->GetPage(i));
        if (page) {
            if (!page->close()) {
                evt.Veto();
            }
        }
    }
    this->Destroy();
}

void MainWindow::enableMenus(const Size id) {
    wxMenuBar* bar = this->GetMenuBar();
    RunPage* page = dynamic_cast<RunPage*>(notebook->GetPage(id));
    if (!runs.contains(page)) {
        bar->EnableTop(1, false);
        // bar->EnableTop(2, false);
        return;
    }
    const bool enableRun = page && runs[page].isRun;
    // const bool enableResult = page && !runs[page].isRun;

    /// \todo avoid hardcoded indices

    bar->EnableTop(1, enableRun);
    //    bar->EnableTop(2, enableResult);
}

NAMESPACE_SPH_END
