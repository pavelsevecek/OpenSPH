#pragma once

#include "gui/Settings.h"
#include "run/Run.h"

NAMESPACE_SPH_BEGIN

class GuiRun : public Abstract::Run {
public:
    virtual GuiSettings getGuiSettings() const = 0;
};

NAMESPACE_SPH_END
