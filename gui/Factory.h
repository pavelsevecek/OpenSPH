#pragma once

#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera;
    class Element;
}
class Palette;
class Point;
enum class ElementId;

namespace Factory {

    /// Creates a camera
    /// \param size Resolution of produced image
    AutoPtr<Abstract::Camera> getCamera(const GuiSettings& settings, const Point size);

    AutoPtr<Abstract::Element> getElement(const GuiSettings& settings, const ElementId id);

    Palette getPalette(const ElementId id, const Interval range);
}

NAMESPACE_SPH_END
