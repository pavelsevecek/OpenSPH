#pragma once

#include "gui/MainLoop.h"
#include "objects/Object.h"
#include "wx/wx.h"
#include <thread>


class wxFrame;

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Run;
    class Renderer;
}
class Window;
class Storage;

template <typename TEnum>
class Settings;
enum class RunSettingsId;
using RunSettings = Settings<RunSettingsId>;

class MyApp : public wxApp {
private:
    Window* window;
    std::thread worker;
    std::unique_ptr<Abstract::Run> run;

    virtual bool OnInit();

    void processEvents(MainLoopEvent& evt);

public:
    MyApp();
    ~MyApp();
};

NAMESPACE_SPH_END
