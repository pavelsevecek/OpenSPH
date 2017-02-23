#pragma once

/// Posting events to be executed on main thread
/// Pavel Sevecek
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <functional>
#include <wx/event.h>

#include "system/LogFile.h"

namespace Sph {
    class MainLoopEvent;
}
wxDECLARE_EVENT(MAIN_LOOP_TYPE, Sph::MainLoopEvent);

typedef void (wxEvtHandler::*MainLoopEventFunction)(Sph::MainLoopEvent&);
#define MainLoopEventHandler(func) wxEVENT_HANDLER_CAST(MainLoopEventFunction, func)

NAMESPACE_SPH_BEGIN

/// Custom event holding callback. Application must handle this event using MainLoopEventFunction and execute
/// callback using MainLoopEvent::execute().
class MainLoopEvent : public wxCommandEvent {
private:
    std::function<void()> callback;

public:
    MainLoopEvent(const std::function<void()>& callback)
        : wxCommandEvent(MAIN_LOOP_TYPE, 0)
        , callback(callback) {}

    MainLoopEvent(const MainLoopEvent& evt)
        : wxCommandEvent(evt)
        , callback(evt.callback) {}

    wxEvent* Clone() const {
        return new MainLoopEvent(*this);
    }

    void execute() {
        callback();
    }
};

/// Post function to main thread.
void executeOnMainThread(const std::function<void()>& function);

NAMESPACE_SPH_END
