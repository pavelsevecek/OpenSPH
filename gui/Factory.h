#pragma once

#include "gui/Settings.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/ExtendedEnum.h"

NAMESPACE_SPH_BEGIN

class ICamera;
class IColorizer;
class IRenderer;
class Palette;
struct Pixel;
class Interval;
class ITracker;
class IBrdf;
class IColorMap;
class IScheduler;
class Project;
enum class ColorizerId;
using ExtColorizerId = ExtendedEnum<ColorizerId>;

namespace Factory {

/// Creates a camera
/// \param size Resolution of produced image
AutoPtr<ICamera> getCamera(const GuiSettings& settings, const Pixel size);

AutoPtr<ITracker> getTracker(const GuiSettings& settings);

AutoPtr<IRenderer> getRenderer(const GuiSettings& settings);

AutoPtr<IRenderer> getRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

AutoPtr<IBrdf> getBrdf(const GuiSettings& settings);

AutoPtr<IColorMap> getColorMap(const GuiSettings& settings);

AutoPtr<IColorizer> getColorizer(const Project& project, const ExtColorizerId id);

Palette getPalette(const ExtColorizerId id);

} // namespace Factory

NAMESPACE_SPH_END
