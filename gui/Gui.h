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

class MyApp : public wxApp {
private:
    Window* window;
    std::thread worker;

    virtual bool OnInit();
    void OnButton(wxCommandEvent& evt);

public:
    ~MyApp();
};

NAMESPACE_SPH_END
