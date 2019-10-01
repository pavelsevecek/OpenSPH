#pragma once

#include "objects/wrappers/Function.h"
#include <wx/timer.h>

NAMESPACE_SPH_BEGIN

class DelayedCallback : private wxTimer {
private:
    Function<void()> callback;

public:
    DelayedCallback() = default;

    void start(const int milliseconds, Function<void()> newCallback) {
        callback = newCallback;
        this->StartOnce(milliseconds);
    }

    void stop() {
        this->Stop();
    }

private:
    virtual void Notify() override {
        callback();
    }
};

NAMESPACE_SPH_END
