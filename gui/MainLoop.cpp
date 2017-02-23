#include "gui/MainLoop.h"
#include <wx/app.h>

wxDEFINE_EVENT(MAIN_LOOP_TYPE, Sph::MainLoopEvent);

NAMESPACE_SPH_BEGIN

void executeOnMainThread(const std::function<void()>& function) {
    MainLoopEvent* evt = new MainLoopEvent(function);
    wxTheApp->QueueEvent(evt);
}

NAMESPACE_SPH_END
