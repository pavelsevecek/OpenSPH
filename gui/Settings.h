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
    NONE = 0,

    /// 2D section showing particles as points
    PARTICLE = 1,

    /// Raymarcher that computes intersections with implicit surface.
    RAYTRACER = 2,

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


/// \brief Special colorizers that do not directly correspond to quantities.
///
/// Must have strictly negative values. Function taking \ref ColorizerId as an argument also acceps \ref
/// QuantityId casted to \ref ColorizerId, interpreting as \ref TypedColorizer with given quantity ID.
enum class ColorizerId {
    VELOCITY = -1,             ///< Particle velocities
    ACCELERATION = -2,         ///< Acceleration of particles
    MOVEMENT_DIRECTION = -3,   ///< Projected direction of motion
    COROTATING_VELOCITY = -4,  ///< Velocities with a respect to the rotating body
    DISPLACEMENT = -5,         ///< Difference between current positions and initial position
    DENSITY_PERTURBATION = -6, ///< Relative difference of density and initial density (rho/rho0 - 1)
    SUMMED_DENSITY = -7,       ///< Density computed from particle masses by direct summation of neighbors
    TOTAL_STRESS = -8,         ///< Total stress (sigma = S - pI)
    TOTAL_ENERGY = -9,         ///< Sum of kinetic and internal energy for given particle
    TEMPERATURE = -10,         ///< Temperature, computed from internal energy
    YIELD_REDUCTION = -11,     ///< Reduction of stress tensor due to yielding (1 - f_vonMises)
    DAMAGE_ACTIVATION = -12,   ///< Ratio of the stress and the activation strain
    RADIUS = -13,              ///< Radii/smoothing lenghts of particles
    UVW = -15,                 ///< Shows UV mapping, u-coordinate in red and v-coordinate in blur
    BOUNDARY = -16,            ///< Shows boundary particles
    PARTICLE_ID = -17,         ///< Each particle drawn with different color
    COMPONENT_ID = -18,        ///< Color assigned to each component (group of connected particles)
    BOUND_COMPONENT_ID = -19,  ///< Color assigned to each group of gravitationally bound particles
    AGGREGATE_ID = -20,        ///< Color assigned to each aggregate
    FLAG = -21,                ///< Particles of different bodies are colored differently
    MATERIAL_ID = -22,         ///< Particles with different materials are colored differently
};

SPH_EXTEND_ENUM(QuantityId, ColorizerId);
using ExtColorizerId = ExtendedEnum<ColorizerId>;

enum class BrdfEnum {
    LAMBERT,
    PHONG,
};

enum class ColorMapEnum {
    LINEAR,
    LOGARITHMIC,
    FILMIC,
};

enum class PaneEnum {
    RENDER_PARAMS = 1 << 0,
    PALETTE = 1 << 1,
    PARTICLE_DATA = 1 << 2,
    PLOTS = 1 << 3,
    STATS = 1 << 4,
};

enum class GuiSettingsId {
    /// Selected renderer
    RENDERER,

    CAMERA_TYPE,

    CAMERA_WIDTH,

    CAMERA_HEIGHT,

    CAMERA_POSITION,

    CAMERA_VELOCITY,

    CAMERA_ORBIT,

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

    COLORMAP_TYPE,

    COLORMAP_LOGARITHMIC_FACTOR,

    REDUCE_LOWFREQUENCY_NOISE,

    BLOOM_INTENSITY,

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

    RAYTRACE_SHADOWS,

    RAYTRACE_SPHERES,

    VOLUME_MAX_DISTENTION,

    VOLUME_EMISSION,

    VOLUME_ABSORPTION,

    CONTOUR_SPACING,

    CONTOUR_GRID_SIZE,

    CONTOUR_SHOW_LABELS,

    DEFAULT_COLORIZER,

    DEFAULT_PANES,

    /// Title of the window appearing on taskbar
    WINDOW_TITLE,

    WINDOW_WIDTH,

    WINDOW_HEIGHT,

    PLOT_INTEGRALS,

    PLOT_INITIAL_PERIOD,

    PLOT_OVERPLOT_SFD,
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
