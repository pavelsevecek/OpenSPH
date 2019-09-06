#include "gui/renderers/IRenderer.h"
#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

void RenderParams::initialize(const GuiSettings& gui) {
    background = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    showKey = gui.get<bool>(GuiSettingsId::SHOW_KEY);
    particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    particles.grayScale = gui.get<bool>(GuiSettingsId::FORCE_GRAYSCALE);
    particles.doAntialiasing = gui.get<bool>(GuiSettingsId::ANTIALIASED);
    particles.smoothed = gui.get<bool>(GuiSettingsId::SMOOTH_PARTICLES);
    particles.renderGhosts = gui.get<bool>(GuiSettingsId::RENDER_GHOST_PARTICLES);
    surface.level = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    surface.ambientLight = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
    surface.sunLight = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
    surface.emission = gui.get<Float>(GuiSettingsId::SURFACE_EMISSION);
    contours.isoStep = gui.get<Float>(GuiSettingsId::CONTOUR_SPACING);
    contours.gridSize = gui.get<int>(GuiSettingsId::CONTOUR_GRID_SIZE);
    contours.showLabels = gui.get<bool>(GuiSettingsId::CONTOUR_SHOW_LABELS);
}

NAMESPACE_SPH_END
