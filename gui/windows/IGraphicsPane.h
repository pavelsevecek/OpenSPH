#pragma once

/// \file IGraphicsPane.h
/// \brief Base class for drawing particles into the window
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Assert.h"
#include "gui/objects/Camera.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class Storage;
class Statistics;

class IGraphicsPane : public wxPanel {
public:
    IGraphicsPane(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize) {}

    virtual ICamera& getCamera() = 0;

    virtual void resetView() = 0;

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) = 0;
};

NAMESPACE_SPH_END
