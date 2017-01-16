#pragma once

#include "objects/Object.h"
#include "wx/wx.h"
#include <thread>

class wxFrame;

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Renderer;
}
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

    void initialConditions(const GlobalSettings& globalSettings, const std::shared_ptr<Storage>& storage);

    virtual bool OnInit();

public:
    ~MyApp();
};

NAMESPACE_SPH_END
