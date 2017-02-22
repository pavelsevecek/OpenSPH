#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class RendererEnum {
    /// 2D section through 3D domain
    ORTHO,

    /// 3D visualization of simulation using OpenGL
    OPENGL
};

enum class OrthoEnum { XY, XZ, YZ };

/// \todo generic ortho projection (x,y,z) -> (u,v)

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

    ORTHO_PROJECTION,

    /// Title of the window appearing on taskbar
    WINDOW_TITLE,

    /// If true, rendered images are saved to disk
    IMAGES_SAVE,

    /// Path of the directory where rendered images are saved
    IMAGES_PATH,

    /// Time interval (in run time) of image saving. Note that images do not have to be saved exactly in
    /// specified times, as time step of the run is generally different than this value.
    IMAGES_TIMESTEP,

    PALETTE_DENSITY,

    PALETTE_VELOCITY,

    PALETTE_PRESSURE,

    PALETTE_ENERGY,

    PALETTE_STRESS,

    PALETTE_DAMAGE
};

using GuiSettings = Settings<GuiSettingsIds>;

NAMESPACE_SPH_END
