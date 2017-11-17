#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class RendererEnum {
    /// 2D section showing particles as points
    PARTICLE,

    /// Reconstructed surface of bodies
    SURFACE,

    /// 3D visualization of simulation using OpenGL
    OPENGL
};

enum class OrthoEnum { XY, XZ, YZ };

enum class IntegralEnum {
    INTERNAL_ENERGY = 1 << 0,
    KINETIC_ENERGY = 1 << 1,
    TOTAL_ENERGY = 1 << 2,
    TOTAL_MOMENTUM = 1 << 3,
    TOTAL_ANGULAR_MOMENTUM = 1 << 4,

    ALL = INTERNAL_ENERGY | KINETIC_ENERGY | TOTAL_ENERGY | TOTAL_MOMENTUM | TOTAL_ANGULAR_MOMENTUM,
};

/// \todo generic ortho projection (x,y,z) -> (u,v)

enum class GuiSettingsId {
    /// Selected renderer
    RENDERER,

    /// Center point of the view
    VIEW_CENTER,

    /// View field of view (zoom)
    VIEW_FOV,

    VIEW_WIDTH,

    VIEW_HEIGHT,

    VIEW_MAX_FRAMERATE,

    /// Displayed radius of particle in units of smoothing length
    PARTICLE_RADIUS,

    /// Max z-coordinate of particle to be displayed by ortho renderer
    ORTHO_CUTOFF,

    ORTHO_PROJECTION,

    /// Size of the grid used in MarchingCubes (in code units, not h)
    SURFACE_RESOLUTION,

    /// Value of iso-surface being constructed; lower value means larget bodies
    SURFACE_LEVEL,

    /// Direction to the sun used for shading
    SURFACE_SUN_POSITION,

    /// Intentity of the sun
    SURFACE_SUN_INTENSITY,

    /// Ambient color for surface renderer
    SURFACE_AMBIENT,

    /// Title of the window appearing on taskbar
    WINDOW_TITLE,

    WINDOW_WIDTH,

    WINDOW_HEIGHT,

    RENDER_WIDTH,

    RENDER_HEIGHT,

    PLOT_INTEGRALS,

    IMAGES_RENDERER,

    IMAGES_WIDTH,

    IMAGES_HEIGHT,

    /// If true, rendered images are saved to disk
    IMAGES_SAVE,

    /// Path of directory where the rendered images will be saved.
    IMAGES_PATH,

    /// Mask of the image names, having %d where the output number will be placed.
    IMAGES_NAME,

    /// Time interval (in run time) of image saving. Note that images do not have to be saved exactly in
    /// specified times, as time step of the run is generally different than this value.
    IMAGES_TIMESTEP,

    PALETTE_DENSITY,

    PALETTE_VELOCITY,

    PALETTE_ACCELERATION,

    PALETTE_PRESSURE,

    PALETTE_ENERGY,

    PALETTE_STRESS,

    PALETTE_DAMAGE,

    PALETTE_DIVV,

    PALETTE_DENSITY_PERTURB,

    PALETTE_RADIUS,
};

using GuiSettings = Settings<GuiSettingsId>;

NAMESPACE_SPH_END
