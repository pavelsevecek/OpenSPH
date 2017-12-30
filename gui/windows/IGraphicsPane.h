#pragma once

/// \file IGraphicsPane.h
/// \brief Base class for drawing particles into the window
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class IGraphicsPane : public wxPanel {
public:
    IGraphicsPane(wxWindow* parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize) {}

    virtual void resetView() = 0;
};

NAMESPACE_SPH_END