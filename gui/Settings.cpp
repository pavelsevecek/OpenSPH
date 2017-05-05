#include "gui/Settings.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template<>
AutoPtr<GuiSettings> GuiSettings::instance (new GuiSettings {
    /// Renderer
    { GuiSettingsId::RENDERER,         "renderer",         int(RendererEnum::ORTHO) },
    { GuiSettingsId::RENDER_WIDTH,     "render.width",     800 },
    { GuiSettingsId::RENDER_HEIGHT,    "render.height",    600 },

    /// Particle visualization
    { GuiSettingsId::PARTICLE_RADIUS,  "particle_radius",  0.5_f },
    { GuiSettingsId::ORTHO_CUTOFF,     "ortho.cutoff",     0.1_f },
    { GuiSettingsId::ORTHO_PROJECTION, "ortho.projection", int(OrthoEnum::XY) },
    { GuiSettingsId::VIEW_CENTER,      "view.center",      Vector(0._f) },
    { GuiSettingsId::VIEW_FOV,         "view.fov",         1._f },
    { GuiSettingsId::VIEW_WIDTH,       "view.width",       800 },
    { GuiSettingsId::VIEW_HEIGHT,      "view.height",      600 },

    /// Window settings
    { GuiSettingsId::WINDOW_TITLE,     "window.title",     std::string("SPH") },
    { GuiSettingsId::WINDOW_WIDTH,     "window.width",     800 },
    { GuiSettingsId::WINDOW_HEIGHT,    "window.height",    600 },

    /// Saved animation frames
    { GuiSettingsId::IMAGES_SAVE,      "images.save",      false },
    { GuiSettingsId::IMAGES_PATH,      "images.path",      std::string("imgs/") },
    { GuiSettingsId::IMAGES_NAME,      "images.name",      std::string("img_%e_%d.png") },
    { GuiSettingsId::IMAGES_TIMESTEP,  "images.time_step", 0.1_f },

    /// Color palettes
    { GuiSettingsId::PALETTE_DENSITY,  "palette.density",  Range(2000._f, 3000._f) },
    { GuiSettingsId::PALETTE_VELOCITY, "palette.velocity", Range(0._f, 1._f) },
    { GuiSettingsId::PALETTE_PRESSURE, "palette.pressure", Range(-1000._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_ENERGY,   "palette.energy",   Range(0._f, 1.e6_f) },
    { GuiSettingsId::PALETTE_STRESS,   "palette.stress",   Range(0._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_DAMAGE,   "palette.damage",   Range(0._f, 1._f) },
    { GuiSettingsId::PALETTE_VELOCITY_DIVERGENCE, "palette.velocity_divergence", Range(-2._f, 2._f) },
});
// clang-format on

/// \todo do we really need to specialize the function here to avoid linker error?
template <>
const Settings<GuiSettingsId>& Settings<GuiSettingsId>::getDefaults() {
    ASSERT(instance != nullptr);
    return *instance;
}

// Explicit instantiation
template class Settings<GuiSettingsId>;

NAMESPACE_SPH_END
