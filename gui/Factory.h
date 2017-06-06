#pragma once

#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera;
    class Element;
}
class Palette;
enum class ElementId;

namespace Factory {
    AutoPtr<Abstract::Camera> getCamera(const GuiSettings& settings);

    AutoPtr<Abstract::Element> getElement(const GuiSettings& settings, const QuantityId id);

    Palette getPalette(const ElementId id, const Range range);
}

NAMESPACE_SPH_END
