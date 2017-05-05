#pragma once

#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera;
    class Element;
}

namespace Factory {
    AutoPtr<Abstract::Camera> getCamera(const GuiSettings& settings);

    AutoPtr<Abstract::Element> getElement(const GuiSettings& settings, const QuantityId id);
}

NAMESPACE_SPH_END
