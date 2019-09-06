#pragma once

#include "common/ForwardDecl.h"
#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "objects/geometry/Vector.h"
#include "objects/utility/EnumMap.h"
#include "objects/wrappers/Function.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class RendererEnum {
    /// No particle visualization
    NONE,

    /// 2D section showing particles as points
    PARTICLE,

    /// Surfaces of bodies are meshed using Marching cubes and drawed as triangles.
    MESH,

    /// Raytracer that computes intersections with implicit surface.
    RAYTRACER,

    /// Draws contours (iso-lines) of quantities
    CONTOUR,

};
static RegisterEnum<RendererEnum> sRenderer({
    { RendererEnum::NONE, "none", "No particle visualization" },
    { RendererEnum::PARTICLE, "particle", "Particles are visualized as circles. No shading." },
    { RendererEnum::MESH,
        "mesh",
        "Surfaces of bodies are meshed using Marching cubes and drawed as triangles." },
    { RendererEnum::RAYTRACER, "raytracer", "Use raytracing to find intersections with implicit surface." },
    { RendererEnum::CONTOUR, "contour", "Draws contours (iso-lines) of quantities" },
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
    /// Evolution of the total internal energy in time
    INTERNAL_ENERGY = 1 << 0,

    /// Evolution of the total kinetic energy in time
    KINETIC_ENERGY = 1 << 1,

    /// Evolution of the total energy (sum of total kinetic energy and total internal energy) in time
    /// \todo Currently does not contain potential energy!
    TOTAL_ENERGY = 1 << 2,

    /// Evolution of the total momentum in time
    TOTAL_MOMENTUM = 1 << 3,

    /// Evolution of the total angular momentum in time
    TOTAL_ANGULAR_MOMENTUM = 1 << 4,

    /// Evolution of the selected quantity for the selected particle in time.
    SELECTED_PARTICLE = 1 << 10,

    ALL = INTERNAL_ENERGY | KINETIC_ENERGY | TOTAL_ENERGY | TOTAL_MOMENTUM | TOTAL_ANGULAR_MOMENTUM |
          SELECTED_PARTICLE,
};
static RegisterEnum<PlotEnum> sPlot({
    { PlotEnum::INTERNAL_ENERGY, "internal_energy", "Plots the total internal energy." },
    { PlotEnum::KINETIC_ENERGY, "kinetic_energy", "Plots the total kinetic energy." },
    { PlotEnum::TOTAL_ENERGY, "total_energy", "Plots the sum of the internal and kinetic energy." },
    { PlotEnum::TOTAL_MOMENTUM, "total_momentum", "Plots the total momentum." },
    { PlotEnum::TOTAL_ANGULAR_MOMENTUM, "total_angular_momentum", "Plots the total angular momentum." },
    /* { PlotEnum::PARTICLE_SFD,
         "particle_sfd",
         "Current cumulative size-frequency distribution of bodies in the simulation." },
     { PlotEnum::PREDICTED_SFD,
         "predicted_sfd",
         "Size-frequency distribution that would be realized if we merged all particles that are currently "
         "gravitationally bounded. It allows to roughly predict the final SFD after reaccumulation. Useful "
         "for both NBody solvers and SPH solvers." },
     { PlotEnum::CURRENT_SFD,
         "current_sfd",
         "Size-frequency distribution of particle components, i.e. groups of particles in mutual contact. "
         "Useful for both NBody solvers and SPH solvers. Note that construcing the SFD has non-negligible "
         "overhead, so it is recommended to specify plot period significantly larger than the time step." },*/
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

    /// View field of view (zoom). Special value 0 means the field of view is computed from the bounding box.
    ORTHO_FOV,

    /// Z-offset of the camera (from origin)
    /// \todo replace this and ORTHO_VIEW_CENTER with camera position
    ORTHO_ZOFFSET,

    PERSPECTIVE_FOV,

    PERSPECTIVE_POSITION,

    PERSPECTIVE_TARGET,

    PERSPECTIVE_UP,

    PERSPECTIVE_CLIP_NEAR,

    PERSPECTIVE_CLIP_FAR,

    PERSPECTIVE_TRACKED_PARTICLE,

    VIEW_WIDTH,

    VIEW_HEIGHT,

    VIEW_MAX_FRAMERATE,

    REFRESH_ON_TIMESTEP,

    /// Size of the grid cell in simulation units (not window units); if zero, no grid is drawn
    VIEW_GRID_SIZE,

    BACKGROUND_COLOR,

    SHOW_KEY,

    FORCE_GRAYSCALE,

    ANTIALIASED,

    SMOOTH_PARTICLES,

    RENDER_GHOST_PARTICLES,

    /// Displayed radius of particle in units of smoothing length
    PARTICLE_RADIUS,

    /// Max z-coordinate of particle to be displayed by ortho renderer
    ORTHO_CUTOFF,

    ORTHO_PROJECTION,

    /// Size of the grid used in MarchingCubes (in code units, not h).
    SURFACE_RESOLUTION,

    /// Value of iso-surface being constructed; lower value means larget bodies
    SURFACE_LEVEL,

    /// Direction to the sun used for shading
    SURFACE_SUN_POSITION,

    /// Intentity of the sun
    SURFACE_SUN_INTENSITY,

    SURFACE_EMISSION,

    /// Ambient color for surface renderer
    SURFACE_AMBIENT,

    RAYTRACE_SUBSAMPLING,

    RAYTRACE_ITERATION_LIMIT,

    RAYTRACE_HDRI,

    RAYTRACE_TEXTURE_PRIMARY,

    RAYTRACE_TEXTURE_SECONDARY,

    CONTOUR_SPACING,

    CONTOUR_GRID_SIZE,

    CONTOUR_SHOW_LABELS,

    /// Title of the window appearing on taskbar
    WINDOW_TITLE,

    WINDOW_WIDTH,

    WINDOW_HEIGHT,

    PLOT_INTEGRALS,

    PLOT_INITIAL_PERIOD,

    PLOT_OVERPLOT_SFD,

    IMAGES_RENDERER,

    IMAGES_WIDTH,

    IMAGES_HEIGHT,

    /// If true, rendered images are saved to disk
    IMAGES_SAVE,

    /// Path of directory where the rendered images will be saved.
    IMAGES_PATH,

    /// Mask of the image names, having %d where the output number will be placed.
    IMAGES_NAME,

    IMAGES_FIRST_INDEX,

    IMAGES_MAKE_MOVIE,

    IMAGES_MOVIE_NAME,

    /// Time interval (in run time) of image saving. Note that images do not have to be saved exactly in
    /// specified times, as time step of the run is generally different than this value.
    IMAGES_TIMESTEP,

};

class GuiSettings : public Settings<GuiSettingsId> {
public:
    using Settings<GuiSettingsId>::Settings;

    Function<void(GuiSettingsId id)> accessor;

    template <typename TValue>
    INLINE TValue get(const GuiSettingsId id) const {
        return Settings<GuiSettingsId>::get<TValue>(id);
    }

    template <typename TValue>
    INLINE GuiSettings& set(const GuiSettingsId id, const TValue& value) {
        Settings<GuiSettingsId>::set(id, value);

        if (accessor) {
            accessor(id);
        }
        return *this;
    }
};

template <>
INLINE Pixel GuiSettings::get<Pixel>(const GuiSettingsId id) const {
    const Interval i = this->get<Interval>(id);
    return Pixel(i.lower(), i.upper());
}

template <>
INLINE Rgba GuiSettings::get<Rgba>(const GuiSettingsId id) const {
    const Vector v = this->get<Vector>(id);
    return Rgba(v[X], v[Y], v[Z], v[H]);
}

template <>
INLINE GuiSettings& GuiSettings::set<Rgba>(const GuiSettingsId id, const Rgba& color) {
    this->set(id, Vector(color.r(), color.g(), color.b(), color.a()));
    return *this;
}

NAMESPACE_SPH_END
