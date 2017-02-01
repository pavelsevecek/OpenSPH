#include "gui/Settings.h"

NAMESPACE_SPH_BEGIN

// Explicit instantiation
template class Settings<GuiSettingsIds>;

// clang-format off
template<>
std::unique_ptr<GuiSettings> GuiSettings::instance (new GuiSettings {
    { GuiSettingsIds::RENDERER,         "renderer",         int(RendererEnum::ORTHO) },
    { GuiSettingsIds::PARTICLE_RADIUS,  "particle_radius",  0.5_f },
    { GuiSettingsIds::ORTHO_CUTOFF,     "ortho.cutoff",     0.1_f },
    { GuiSettingsIds::VIEW_CENTER,      "view.center",      Vector(0._f) },
    { GuiSettingsIds::VIEW_FOV,         "view.fov",         1._f },
    { GuiSettingsIds::WINDOW_TITLE,     "window.title",     std::string("SPH") },
    { GuiSettingsIds::PALETTE_DENSITY,  "palette.density",  Range(2000._f, 3000._f) },
    { GuiSettingsIds::PALETTE_VELOCITY, "palette.velocity", Range(0._f, 1._f) },
    { GuiSettingsIds::PALETTE_PRESSURE, "palette.pressure", Range(-1000._f, 1.e10_f) },
    { GuiSettingsIds::PALETTE_ENERGY,   "palette.energy",   Range(0._f, 1.e6_f) },
    { GuiSettingsIds::PALETTE_STRESS,   "palette.stress",   Range(0._f, 1.e10_f) },
    { GuiSettingsIds::PALETTE_DAMAGE,   "palette.damage",   Range(0._f, 1._f) },
});
// clang-format on

NAMESPACE_SPH_END
