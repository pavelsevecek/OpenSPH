#pragma once

#include "gui/Project.h"
#include "gui/Settings.h"
#include "io/Path.h"
#include "objects/wrappers/RawPtr.h"
#include <wx/frame.h>

class wxMenu;
class wxAuiManager;
class wxAuiNotebook;

NAMESPACE_SPH_BEGIN

class Controller;
class IPluginControls;
class RunPage;
class NodeWindow;
class INode;

wxAuiNotebook* findNotebook();

class MainWindow : public wxFrame {
    friend class NodeManagerCallbacks;

private:
    wxAuiNotebook* notebook;
    NodeWindow* nodePage;

    struct RunData {
        AutoPtr<Controller> controller;
        bool isRun;
    };

    FlatMap<RunPage*, RunData> runs;

    Path projectPath;
    bool savedFlag = true;
    wxMenu* runMenu;

public:
    MainWindow(const Path& openPath = Path());

private:
    wxMenu* createProjectMenu();

    wxMenu* createRunMenu();

    wxMenu* createAnalysisMenu();

    wxMenu* createResultMenu();

    void addPage(SharedPtr<INode> node, const RunSettings& globals, const std::string pageName);

    bool removeAll();

    void saveAs();

    void save();

    void open(const Path& openPath, const bool setDefaults);

    void load(const Path& openPath = Path(""));

    void setProjectPath(const Path& newPath);

    void markSaved(const bool saved);

    void onClose(wxCloseEvent& evt);

    void enableMenus(const Size id);

    void enableRunMenu(const bool enableControls, const bool enableCamera);

    bool closeRun(const Size id);

    int checkUnsavedSession();
};

NAMESPACE_SPH_END
