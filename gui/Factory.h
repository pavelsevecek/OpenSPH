#pragma once

#include "gui/Settings.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

class ICamera;
class IColorizer;
class IRenderer;
class Palette;
class Point;
class Interval;
class IBrdf;
class IScheduler;
enum class ColorizerId;

namespace Factory {

    /// Creates a camera
    /// \param size Resolution of produced image
    AutoPtr<ICamera> getCamera(const GuiSettings& settings, const Point size);

    AutoPtr<IRenderer> getRenderer(IScheduler& scheduler, const GuiSettings& settings);

    AutoPtr<IBrdf> getBrdf(const GuiSettings& settings);

    AutoPtr<IColorizer> getColorizer(const GuiSettings& settings,
        const ColorizerId id,
        const FlatMap<ColorizerId, Palette>& paletteOverrides = {});

    Palette getPalette(const ColorizerId id, const Interval range);
} // namespace Factory

NAMESPACE_SPH_END
