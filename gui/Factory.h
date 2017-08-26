#pragma once

#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

class ICamera;
class IColorizer;
class Palette;
class Point;
enum class ColorizerId;

namespace Factory {

    /// Creates a camera
    /// \param size Resolution of produced image
    AutoPtr<ICamera> getCamera(const GuiSettings& settings, const Point size);

    AutoPtr<IColorizer> getColorizer(const GuiSettings& settings, const ColorizerId id);

    Palette getPalette(const ColorizerId id, const Interval range);
}

NAMESPACE_SPH_END
