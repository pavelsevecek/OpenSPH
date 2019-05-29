#include "gui/renderers/IRenderer.h"
#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

void RenderParams::initialize(const GuiSettings& gui) {
    background = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    particles.grayScale = gui.get<bool>(GuiSettingsId::FORCE_GRAYSCALE);
    particles.doAntialiasing = gui.get<bool>(GuiSettingsId::ANTIALIASED);
    particles.smoothed = gui.get<bool>(GuiSettingsId::SMOOTH_PARTICLES);
    particles.showKey = gui.get<bool>(GuiSettingsId::SHOW_KEY);
    surface.level = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    surface.ambientLight = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
    surface.sunLight = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
}

NAMESPACE_SPH_END