#pragma once

#include "gui/windows/ProgressPanel.h"
#include "gui/windows/Widgets.h"
#include "objects/wrappers/AutoPtr.h"
#include <thread>
#include <wx/panel.h>

class wxAuiManager;

NAMESPACE_SPH_BEGIN

class ImagePane;
class INode;
class RenderPageCallbacks;

class RenderPage : public ClosablePage {
private:
    AutoPtr<wxAuiManager> manager;
    AutoPtr<RenderPageCallbacks> callbacks;

    std::thread renderThread;
    std::atomic_bool running;

public:
    RenderPage(wxWindow* parent, const RunSettings& global, const SharedPtr<INode>& node);

    ~RenderPage();

private:
    // ClosablePage interface
    virtual bool isRunning() const override;
    virtual void stop() override;
    virtual void quit() override;
};

NAMESPACE_SPH_END
