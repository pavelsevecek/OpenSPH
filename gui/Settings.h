#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class RendererEnum {
    /// 2D section through 3D domain
    ORTHO,

    /// 3D visualization of simulation using OpenGL
    OPENGL
};

enum class GuiSettingsIds {
    /// Selected renderer
    RENDERER,

    /// Center point of the view
    VIEW_CENTER,

    /// View field of view (zoom)
    VIEW_FOV,

    /// Displayed radius of particle in units of smoothing length
    PARTICLE_RADIUS,

    /// Max z-coordinate of particle to be displayed by ortho renderer
    ORTHO_CUTOFF,

    /// Title of the window appearing on taskbar
    WINDOW_TITLE,

    PALETTE_DENSITY,

    PALETTE_VELOCITY,

    PALETTE_PRESSURE,

    PALETTE_DAMAGE

};

// clang-format off
const Settings<GuiSettingsIds> GUI_SETTINGS = {
    { GuiSettingsIds::RENDERER,         "renderer",         int(RendererEnum::ORTHO) },
    { GuiSettingsIds::PARTICLE_RADIUS,  "particle_radius",  0.5_f },
    { GuiSettingsIds::ORTHO_CUTOFF,     "ortho.cutoff",     0.1_f },
    { GuiSettingsIds::VIEW_CENTER,      "view.center",      Vector(0._f) },
    { GuiSettingsIds::VIEW_FOV,         "view.fov",         1._f },
    { GuiSettingsIds::WINDOW_TITLE,     "window.title",     std::string("SPH") },
    { GuiSettingsIds::PALETTE_DENSITY,  "palette.density",  Range(1000._f, 3000._f) },
    { GuiSettingsIds::PALETTE_VELOCITY, "palette.velocity", Range(0._f, 1._f) },
    { GuiSettingsIds::PALETTE_PRESSURE, "palette.pressure", Range(-1000._f, 1.e7_f) },
    { GuiSettingsIds::PALETTE_DAMAGE,   "palette.damage",   Range(0._f, 1._f) },
};
// clang-format on

using GuiSettings = Settings<GuiSettingsIds>;

NAMESPACE_SPH_END
