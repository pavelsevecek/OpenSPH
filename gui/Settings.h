#pragma once

#include "common/ForwardDecl.h"
#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "objects/geometry/Vector.h"
#include "objects/utility/EnumMap.h"
#include "objects/wrappers/ExtendedEnum.h"
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

    /// Raymarcher that computes intersections with implicit surface.
    RAYMARCHER,

    /// Volumetric renderer
    VOLUME,

    /// Draws contours (iso-lines) of quantities
    CONTOUR,

};

enum class CameraEnum {
    ORTHO,
    PERSPECTIVE,
    FISHEYE,
    SPHERICAL,
};

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

    /// Relative change of total energy
    RELATIVE_ENERGY_CHANGE = 1 << 5,

    /// Current size-frequency distribution
    CURRENT_SFD = 1 << 6,

    /// Predicted size-frequency distribution
    PREDICTED_SFD = 1 << 7,

    /// Speed histogram
    SPEED_HISTOGRAM = 1 << 8,

    /// Angular histogram (in x-y plane) of velocity directions
    ANGULAR_HISTOGRAM_OF_VELOCITIES = 1 << 9,

    /// Evolution of the selected quantity for the selected particle in time.
    SELECTED_PARTICLE = 1 << 10,

    ALL = INTERNAL_ENERGY | KINETIC_ENERGY | TOTAL_ENERGY | TOTAL_MOMENTUM | TOTAL_ANGULAR_MOMENTUM,
};

enum class BrdfEnum {
    LAMBERT,
    PHONG,
};

enum class ColorMapEnum {
    LINEAR,
    LOGARITHMIC,
    FILMIC,
};

enum class GuiSettingsId {
    /// Selected renderer
    RENDERER,

    CAMERA_TYPE,

    CAMERA_POSITION,

    CAMERA_VELOCITY,

    CAMERA_TARGET,

    CAMERA_UP,

    /// View field of view (zoom). Special value 0 means the field of view is computed from the bounding box.
    CAMERA_ORTHO_FOV,

    CAMERA_PERSPECTIVE_FOV,

    CAMERA_CLIP_NEAR,

    CAMERA_CLIP_FAR,

    CAMERA_AUTOSETUP,

    CAMERA_TRACK_PARTICLE,

    CAMERA_TRACK_MEDIAN,

    CAMERA_TRACKING_OFFSET,

    VIEW_WIDTH,

    VIEW_HEIGHT,

    VIEW_MAX_FRAMERATE,

    REFRESH_ON_TIMESTEP,

    /// Size of the grid cell in simulation units (not window units); if zero, no grid is drawn
    VIEW_GRID_SIZE,

    BACKGROUND_COLOR,

    COLORMAP,

    SHOW_KEY,

    FORCE_GRAYSCALE,

    ANTIALIASED,

    SMOOTH_PARTICLES,

    RENDER_GHOST_PARTICLES,

    /// Displayed radius of particle in units of smoothing length
    PARTICLE_RADIUS,

    /// Max z-coordinate of particle to be displayed by ortho renderer
    CAMERA_ORTHO_CUTOFF,

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

    RAYTRACE_BRDF,

    RAYTRACE_SPHERES,

    VOLUME_EMISSION,

    CONTOUR_SPACING,

    CONTOUR_GRID_SIZE,

    CONTOUR_SHOW_LABELS,

    DEFAULT_COLORIZER,

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
    return Pixel(int(i.lower()), int(i.upper()));
}

template <>
INLINE Rgba GuiSettings::get<Rgba>(const GuiSettingsId id) const {
    const Vector v = this->get<Vector>(id);
    return Rgba(float(v[X]), float(v[Y]), float(v[Z]), float(v[H]));
}

template <>
INLINE GuiSettings& GuiSettings::set<Rgba>(const GuiSettingsId id, const Rgba& color) {
    this->set(id, Vector(color.r(), color.g(), color.b(), color.a()));
    return *this;
}

NAMESPACE_SPH_END
