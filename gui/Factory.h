#pragma once

#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera;
    class Element;
}

namespace Factory {
    std::unique_ptr<Abstract::Camera> getCamera(const GuiSettings& settings);

    std::unique_ptr<Abstract::Element> getElement(const GuiSettings& settings, const QuantityId id);
}

NAMESPACE_SPH_END
