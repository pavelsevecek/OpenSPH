#pragma once

#include "common/ForwardDecl.h"
#include "objects/utility/EnumMap.h"

NAMESPACE_SPH_BEGIN

/// \todo currently mixing rendered geometry (spheres, mesh from marching cubes, iso-surface) with renderer
/// (wx, opengl, raytracer, ...)
enum class RendererEnum {
    NONE,

    /// 2D section showing particles as points
    PARTICLE,

    /// Reconstructed surface of bodies
    MESH,

    RAYTRACER,

};
static RegisterEnum<RendererEnum> sRenderer({
    { RendererEnum::NONE, "none", "No particle visualization" },
    { RendererEnum::PARTICLE, "particle", "Particles are visualized as circles. No shading." },
    { RendererEnum::MESH,
        "mesh",
        "Surfaces of bodies are meshed using Marching cubes and drawed as triangles." },
    { RendererEnum::RAYTRACER, "raytracer", "Use raytracing to find intersections with implicit surface." },
});

enum class CameraEnum {
    ORTHO,
    PERSPECTIVE,
};
static RegisterEnum<CameraEnum> sCamera({
    { CameraEnum::ORTHO, "ortho", "Orthographic projection" },
    { CameraEnum::PERSPECTIVE, "perspective", "Perspective projection" },
});

enum class OrthoEnum { XY, XZ, YZ };
/// \todo replace with up and dir
static RegisterEnum<OrthoEnum> sOrtho({
    { OrthoEnum::XY, "xy", "XY plane" },
    { OrthoEnum::XZ, "xz", "XZ plane" },
    { OrthoEnum::YZ, "yz", "YZ plane" },
});

enum class PlotEnum {
    INTERNAL_ENERGY = 1 << 0,
    KINETIC_ENERGY = 1 << 1,
    TOTAL_ENERGY = 1 << 2,
    TOTAL_MOMENTUM = 1 << 3,
    TOTAL_ANGULAR_MOMENTUM = 1 << 4,
    SIZE_FREQUENCY_DISTRIBUTION = 1 << 5,
    SELECTED_PARTICLE = 1 << 6,

    ALL = INTERNAL_ENERGY | KINETIC_ENERGY | TOTAL_ENERGY | TOTAL_MOMENTUM | TOTAL_ANGULAR_MOMENTUM |
          SIZE_FREQUENCY_DISTRIBUTION | SELECTED_PARTICLE,
};
static RegisterEnum<PlotEnum> sPlot({
    { PlotEnum::INTERNAL_ENERGY, "internal_energy", "Plots the total internal energy." },
    { PlotEnum::KINETIC_ENERGY, "kinetic_energy", "Plots the total kinetic energy." },
    { PlotEnum::TOTAL_ENERGY, "total_energy", "Plots the sum of the internal and kinetic energy." },
    { PlotEnum::TOTAL_MOMENTUM, "total_momentum", "Plots the total momentum." },
    { PlotEnum::TOTAL_ANGULAR_MOMENTUM, "total_angular_momentum", "Plots the total angular momentum." },
    { PlotEnum::SIZE_FREQUENCY_DISTRIBUTION,
        "size_distribution",
        "Current cumulative size-frequency distribution of bodies in the simulation." },
    { PlotEnum::SELECTED_PARTICLE,
        "selected_particle",
        "Plots the current quantity of the selected particle." },
});

/// \todo generic ortho projection (x,y,z) -> (u,v)

enum class GuiSettingsId {
    /// Selected renderer
    RENDERER,

    CAMERA,

    /// Center point of the view
    ORTHO_VIEW_CENTER,

    /// View field of view (zoom)
    ORTHO_FOV,

    /// Z-offset of the camera (from origin)
    /// \todo replace this and ORTHO_VIEW_CENTER with camera position
    ORTHO_ZOFFSET,

    PERSPECTIVE_FOV,

    PERSPECTIVE_POSITION,

    PERSPECTIVE_TARGET,

    PERSPECTIVE_UP,

    VIEW_WIDTH,

    VIEW_HEIGHT,

    VIEW_MAX_FRAMERATE,

    /// Size of the grid cell in simulation units (not window units); if zero, no grid is drawn
    VIEW_GRID_SIZE,

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

    RAYTRACE_SUBSAMPLING,

    RAYTRACE_HDRI,

    RAYTRACE_TEXTURE_PRIMARY,

    RAYTRACE_TEXTURE_SECONDARY,

    /// Title of the window appearing on taskbar
    WINDOW_TITLE,

    WINDOW_WIDTH,

    WINDOW_HEIGHT,

    PLOT_INTEGRALS,

    PLOT_INITIAL_PERIOD,

    IMAGES_RENDERER,

    IMAGES_WIDTH,

    IMAGES_HEIGHT,

    /// If true, rendered images are saved to disk
    IMAGES_SAVE,

    /// Path of directory where the rendered images will be saved.
    IMAGES_PATH,

    /// Mask of the image names, having %d where the output number will be placed.
    IMAGES_NAME,

    IMAGES_MAKE_MOVIE,

    IMAGES_MOVIE_NAME,

    /// Time interval (in run time) of image saving. Note that images do not have to be saved exactly in
    /// specified times, as time step of the run is generally different than this value.
    IMAGES_TIMESTEP,

    PALETTE_DENSITY,

    PALETTE_MASS,

    PALETTE_VELOCITY,

    PALETTE_ACCELERATION,

    PALETTE_PRESSURE,

    PALETTE_ENERGY,

    PALETTE_STRESS,

    PALETTE_DAMAGE,

    PALETTE_DIVV,

    PALETTE_GRADV,

    PALETTE_ROTV,

    PALETTE_DENSITY_PERTURB,

    PALETTE_RADIUS,

    PALETTE_ANGULAR_VELOCITY,

    PALETTE_MOMENT_OF_INERTIA,

    PALETTE_STRAIN_RATE_CORRECTION_TENSOR,

    PALETTE_VELOCITY_SECOND_DERIVATIVES,

    PALETTE_ACTIVATION_STRAIN,

    PALETTE_TOTAL_ENERGY,
};

using GuiSettings = Settings<GuiSettingsId>;

NAMESPACE_SPH_END
