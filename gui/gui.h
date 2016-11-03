#pragma once

#include "wx/wx.h"
#include <thread>

namespace Sph {
    namespace Gui {
        class CustomGlPane;
    }
}

class wxFrame;


class MyApp : public wxApp {
private:
    wxFrame* frame;
    Sph::Gui::CustomGlPane* glPane;
    std::thread worker;

    virtual bool OnInit();
    void OnButton(wxCommandEvent& evt);
public:
    ~MyApp();
};
