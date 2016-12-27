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

    /// Displayed radius of particle in units of smoothing length
    PARTICLE_RADIUS,

    /// Max z-coordinate of particle to be displayed by ortho renderer
    ORTHO_CUTOFF,

    /// Center point of the view
    VIEW_CENTER,

    /// View field of view (zoom)
    VIEW_FOV
};

// clang-format off
const Settings<GuiSettingsIds> GUI_SETTINGS = {
    { GuiSettingsIds::RENDERER,         "renderer",         int(RendererEnum::ORTHO) },
    { GuiSettingsIds::PARTICLE_RADIUS,  "particle_radius",  0.5_f },
    { GuiSettingsIds::ORTHO_CUTOFF,     "ortho.cutoff",     0.1_f },
    { GuiSettingsIds::VIEW_CENTER,      "view.center",      Vector(0._f) },
    { GuiSettingsIds::VIEW_FOV,         "view.fov",         1._f }
};
// clang-format on

NAMESPACE_SPH_END
