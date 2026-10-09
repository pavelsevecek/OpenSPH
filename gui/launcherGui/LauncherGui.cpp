#include "gui/launcherGui/LauncherGui.h"
#include "gui/Theme.h"
#include "gui/windows/MainWindow.h"
#include <wx/msgdlg.h>
#include <wx/weakref.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

bool App::OnInit() {
#if wxCHECK_VERSION(3, 3, 0)
    this->SetAppearance(wxApp::Appearance::Dark);
#endif
#ifdef __WXMSW__
    DarkTheme::windowsTheme();
#endif
#ifndef SPH_DEBUG
    wxDisableAsserts();
#endif

    this->Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));

    if (wxTheApp->argc > 1) {
        Path path(String::fromUtf8(wxTheApp->argv[1]));
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

int App::FilterEvent(wxEvent& event) {
    if (event.GetEventType() == wxEVT_CREATE) {
        if (wxWindow* created = dynamic_cast<wxWindow*>(event.GetEventObject())) {
            DarkTheme::apply(created);
            // Native controls may overwrite their colours later in Create().
            this->CallAfter([weak = wxWeakRef<wxWindow>(created)] {
                if (weak) {
                    DarkTheme::applyTree(weak.get());
                }
            });
        }
    } else if (event.GetEventType() == wxEVT_SHOW && static_cast<wxShowEvent&>(event).IsShown()) {
        if (wxWindow* shown = dynamic_cast<wxWindow*>(event.GetEventObject())) {
            DarkTheme::applyTree(shown);
        }
    }
    return wxApp::FilterEvent(event);
}

NAMESPACE_SPH_END
