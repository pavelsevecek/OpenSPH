#pragma once

#include "objects/Object.h"
#include "wx/wx.h"
#include <thread>

class wxFrame;

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Renderer;
}
class Problem;
class Window;
class Storage;

template <typename TEnum>
class Settings;
enum class GlobalSettingsIds;
using GlobalSettings = Settings<GlobalSettingsIds>;

class MyApp : public wxApp {
private:
    Window* window;
    std::thread worker;
    std::unique_ptr<Problem> p;

    virtual bool OnInit();

public:
    MyApp();
    ~MyApp();
};

NAMESPACE_SPH_END
