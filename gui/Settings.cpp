#include "gui/Settings.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template<>
std::unique_ptr<GuiSettings> GuiSettings::instance (new GuiSettings {
    { GuiSettingsId::RENDERER,         "renderer",         int(RendererEnum::ORTHO) },
    { GuiSettingsId::PARTICLE_RADIUS,  "particle_radius",  0.5_f },
    { GuiSettingsId::ORTHO_CUTOFF,     "ortho.cutoff",     0.1_f },
    { GuiSettingsId::ORTHO_PROJECTION, "ortho.projection", int(OrthoEnum::XY) },
    { GuiSettingsId::VIEW_CENTER,      "view.center",      Vector(0._f) },
    { GuiSettingsId::VIEW_FOV,         "view.fov",         1._f },
    { GuiSettingsId::WINDOW_TITLE,     "window.title",     std::string("SPH") },
    { GuiSettingsId::IMAGES_SAVE,      "images.save",      false },
    { GuiSettingsId::IMAGES_PATH,      "images.path",      std::string("imgs/") },
    { GuiSettingsId::IMAGES_NAME,      "images.name",      std::string("img_%d.png") },
    { GuiSettingsId::IMAGES_TIMESTEP,  "images.time_step", 0.1_f },
    { GuiSettingsId::PALETTE_DENSITY,  "palette.density",  Range(2000._f, 3000._f) },
    { GuiSettingsId::PALETTE_VELOCITY, "palette.velocity", Range(0._f, 1._f) },
    { GuiSettingsId::PALETTE_PRESSURE, "palette.pressure", Range(-1000._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_ENERGY,   "palette.energy",   Range(0._f, 1.e6_f) },
    { GuiSettingsId::PALETTE_STRESS,   "palette.stress",   Range(0._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_DAMAGE,   "palette.damage",   Range(0._f, 1._f) },
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
