#include "gui/launcherGui/LauncherGui.h"
#include "gui/windows/MainWindow.h"
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

bool App::OnInit() {
#ifndef SPH_DEBUG
    wxDisableAsserts();
#endif

    this->Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    if (wxTheApp->argc > 1) {
        Path path(std::string(wxTheApp->argv[1]));
        window = new MainWindow(path);
    } else {
        window = new MainWindow();
    }
    window->SetAutoLayout(true);
    window->Show();
    return true;
}

int App::OnExit() {
    return 0;
}

NAMESPACE_SPH_END
