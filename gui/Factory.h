#pragma once

#include "gui/Settings.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

class ICamera;
class IColorizer;
class IRenderer;
class Palette;
struct Pixel;
class Interval;
class IBrdf;
class IScheduler;
class Project;
enum class ColorizerId;

namespace Factory {

    /// Creates a camera
    /// \param size Resolution of produced image
    AutoPtr<ICamera> getCamera(const GuiSettings& settings, const Pixel size);

    AutoPtr<IRenderer> getRenderer(IScheduler& scheduler, const GuiSettings& settings);

    AutoPtr<IBrdf> getBrdf(const GuiSettings& settings);

    AutoPtr<IColorizer> getColorizer(const Project& project, const ColorizerId id);

    Palette getPalette(const ColorizerId id);

} // namespace Factory

NAMESPACE_SPH_END
