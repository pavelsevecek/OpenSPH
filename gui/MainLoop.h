#pragma once

/// \file MainLoop.h
/// \brief Posting events to be executed on main thread
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/wrappers/Function.h"

#include <wx/event.h>

namespace Sph {
    class MainLoopEvent;
}
wxDECLARE_EVENT(MAIN_LOOP_TYPE, Sph::MainLoopEvent);

typedef void (wxEvtHandler::*MainLoopEventFunction)(Sph::MainLoopEvent&);
#define MainLoopEventHandler(func) wxEVENT_HANDLER_CAST(MainLoopEventFunction, func)

NAMESPACE_SPH_BEGIN

/// \brief Custom event holding a callback.
///
/// Application must handle this event using MainLoopEventFunction and execute callback using
/// MainLoopEvent::execute().
class MainLoopEvent : public wxCommandEvent {
private:
    Function<void()> callback;

public:
    MainLoopEvent(const Function<void()>& callback)
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

/// \brief Posts a callback to be executed on main thread.
///
/// The function does not wait for the callback to be executed. The callback is executed by wxWidget
/// framework; that means the event loop must be running and there must be an event handler executing the
/// callback.
void executeOnMainThread(const Function<void()>& function);

/// \brief Executes a callback in main thread, passing a shared pointer to given object as its argument.
///
/// The callback is only executed if the object referenced by the shared pointer is not expired, otherwise it
/// is ignored.
template <typename Type, typename TFunctor>
void executeOnMainThread(const SharedPtr<Type>& ptr, TFunctor functor) {
    executeOnMainThread([weakPtr = WeakPtr<Type>(ptr), f = std::move(functor)] {
        if (auto ptr = weakPtr.lock()) {
            f(ptr);
        }
    });
}

NAMESPACE_SPH_END
