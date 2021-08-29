#include "gui/renderers/IRenderer.h"
#include "gui/Factory.h"
#include "gui/ImageTransform.h"
#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "gui/renderers/FrameBuffer.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

void RenderParams::initialize(const GuiSettings& gui) {
    background = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    showKey = gui.get<bool>(GuiSettingsId::SHOW_KEY);
    particles.scale = float(gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS));
    particles.grayScale = gui.get<bool>(GuiSettingsId::FORCE_GRAYSCALE);
    particles.doAntialiasing = gui.get<bool>(GuiSettingsId::ANTIALIASED);
    particles.smoothed = gui.get<bool>(GuiSettingsId::SMOOTH_PARTICLES);
    particles.renderGhosts = gui.get<bool>(GuiSettingsId::RENDER_GHOST_PARTICLES);
    surface.level = float(gui.get<Float>(GuiSettingsId::SURFACE_LEVEL));
    surface.ambientLight = float(gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT));
    surface.sunLight = float(gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY));
    post.compressionFactor = float(gui.get<Float>(GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR));
    post.denoise = gui.get<bool>(GuiSettingsId::REDUCE_LOWFREQUENCY_NOISE);
    post.bloomIntensity = gui.get<Float>(GuiSettingsId::BLOOM_INTENSITY);
}

NAMESPACE_SPH_END
