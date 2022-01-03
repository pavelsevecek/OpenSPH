#pragma once

#include "gui/Settings.h"
#include "io/Path.h"
#include <wx/dialog.h>

NAMESPACE_SPH_BEGIN

class RenderSetup : public wxDialog {
public:
    RenderSetup(wxWindow* parent);

    CameraEnum selectedCamera;
    RendererEnum selectedRenderer;
    Path firstFilePath;
    Path outputDir;
    bool doSequence = false;
    bool doRender = false;
};

NAMESPACE_SPH_END
