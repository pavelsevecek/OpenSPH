#include "gui/Settings.h"
#include "gui/objects/Colorizer.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

static RegisterEnum<RendererEnum> sRenderer({
    { RendererEnum::PARTICLE, "particle", "Particles are visualized as circles. No shading." },
    /*{ RendererEnum::MESH,
        "mesh",
        "Surfaces of bodies are meshed using Marching cubes and drawed as triangles." },*/
    { RendererEnum::RAYMARCHER,
        "raymarcher",
        "Use raymarching to find intersections with implicit surface." },
    { RendererEnum::VOLUME, "volumetric", "Use raytracing to find total emission along the ray." },
    //{ RendererEnum::CONTOUR, "contour", "Draws contours (iso-lines) of quantities" },
});

static RegisterEnum<CameraEnum> sCamera({
    { CameraEnum::ORTHO, "ortho", "Orthographic projection" },
    { CameraEnum::PERSPECTIVE, "perspective", "Perspective projection" },
    { CameraEnum::FISHEYE, "fisheye", "Fisheye equidistant projection" },
    { CameraEnum::SPHERICAL, "spherical", "Spherical 360° projection" },
});

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

static RegisterEnum<BrdfEnum> sBrdf({
    { BrdfEnum::LAMBERT, "lambert", "Lambert shading" },
    { BrdfEnum::PHONG, "phong", "Phong shading" },
});

static RegisterEnum<ColorMapEnum> sColorMap({
    { ColorMapEnum::LINEAR, "linear", "No colormapping transform" },
    { ColorMapEnum::LOGARITHMIC, "logarithmic", "Uses logarithmic transform for color mapping" },
    { ColorMapEnum::FILMIC, "filmic", "Uses filmic color mapping" },
});


// clang-format off
template<>
AutoPtr<Settings<GuiSettingsId>> Settings<GuiSettingsId>::instance (new Settings<GuiSettingsId> {
    /// Camera pameters
    { GuiSettingsId::PARTICLE_RADIUS,       "particle_radius",      0.5_f,
        "Multiplier of the particle radius for drawing." },
    { GuiSettingsId::CAMERA_TYPE,           "camera.type",          CameraEnum::ORTHO,
        "Specifies the projection of the particles to the image. Can be one of the following:\n" + EnumMap::getDesc<CameraEnum>() },
    { GuiSettingsId::CAMERA_WIDTH,          "images.width",         800,
        "Width of the created images." },
    { GuiSettingsId::CAMERA_HEIGHT,         "images.height",        600,
        "Height of the created images." },
    { GuiSettingsId::CAMERA_POSITION,       "camera.position",      Vector(0._f, 0._f, 1.e4_f),
        "Position of the camera in space." },
    { GuiSettingsId::CAMERA_VELOCITY,       "camera.velocity",      Vector(0._f),
        "Velocity of the camera in space." },
    { GuiSettingsId::CAMERA_ORBIT,          "camera.orbit",         0._f,
        "Angular velocity of the camera orbiting arount its target." },
    { GuiSettingsId::CAMERA_TARGET,         "camera.target",        Vector(0._f),
        "Look-at point of the perspective camera. Actual distance from the camera does not matter." },
    { GuiSettingsId::CAMERA_UP,             "camera.up",            Vector(0._f, 1._f, 0._f),
        "Up-vector of the perspective camera. Does not have to be normalized." },
    { GuiSettingsId::CAMERA_CLIP_NEAR,      "camera.clip.near",     EPS,
        "Nearest distance that can be projected by the perspective camera" },
    { GuiSettingsId::CAMERA_CLIP_FAR,       "camera.clip.far",      INFTY,
        "Farthest distance that can be projected by the perspective camera" },
    { GuiSettingsId::CAMERA_PERSPECTIVE_FOV,   "camera.perspective.fov",   PI / 3._f,
        "Field of view of the perspective camera (in radians)." },
    { GuiSettingsId::CAMERA_ORTHO_CUTOFF,   "camera.ortho.cutoff",      0._f,
        "Cut-off distance from center plane. Particles further away are not drawn. Used by particle renderer." },
    { GuiSettingsId::CAMERA_ORTHO_FOV,      "camera.ortho.fov",         1.e5_f,
        "Field of view of the orthographic camera. Specified as distance (not an angle)."},
    { GuiSettingsId::CAMERA_TRACK_PARTICLE, "camera.track_particle",    -1,
        "Index of the particle tracked by the camera. -1 means no tracking is used. " },
    { GuiSettingsId::CAMERA_AUTOSETUP,      "camera.autosetup",         true,
        "If true, camera parameters are automatically adjusted based on particle data. "
        "This overrides other parameters, such as field of view, camera position, etc." },
    { GuiSettingsId::CAMERA_TRACK_MEDIAN,   "camera.track_median",      false,
        "If true, camera tracks the median position of particles. Not used if camera.track_particle is set." },
    { GuiSettingsId::CAMERA_TRACKING_OFFSET, "camera.tracking_offset",  Vector(0._f),
        "Constant offset from the median." },

    /// Particle visualization
    { GuiSettingsId::RENDERER,              "renderer",             RendererEnum::PARTICLE,
        "Selected renderer for particle visualization. Can be one of the following:\n" + EnumMap::getDesc<RendererEnum>() },
    { GuiSettingsId::VIEW_WIDTH,            "view.width",           800,
        "Width of the rendered image (in pixels)." },
    { GuiSettingsId::VIEW_HEIGHT,           "view.height",          600,
        "Height of the rendered image (in pixels)." },
    { GuiSettingsId::VIEW_MAX_FRAMERATE,    "view.max_framerate",   10, // ms
        "Minimal refresh period of the drawed bitmap. Used to avoid visualization unnecessarily affecting "
        "the performance of the simulation." },
    { GuiSettingsId::REFRESH_ON_TIMESTEP,   "view.refresh_on_timestep",  true,
        "If true, the image is automatically refreshed every timestep, otherwise manual refresh is needed." },
    { GuiSettingsId::VIEW_GRID_SIZE,        "view.grid_size",       0._f,
        "Step of the grid drawn into the bitmap. If zero, no grid is drawn." },
    { GuiSettingsId::SURFACE_RESOLUTION,    "surface.resolution",   100._f,  // m
        "Resolution of the meshed surface (in world units). Lower values means the mesh is more detailed, "
        "but construction takes (significantly) more time and memory." },
    { GuiSettingsId::SURFACE_LEVEL,         "surface.level",        0.13_f,
        "Surface level for mesh renderer and raytracer. Specifies the value of the constructed/intersected "
        "iso-surface of color field." },
    { GuiSettingsId::SURFACE_SUN_POSITION,  "surface.sun_position", Vector(0.f, 0.f, 1.f),
        "Direction to the sun, used for shading in mesh renderer in raytracer." },
    { GuiSettingsId::SURFACE_SUN_INTENSITY, "surface.sun_intentity", 0.7_f,
        "Relative intensity of the sun, used for shading in mesh renderer in raytracer." },
    { GuiSettingsId::SURFACE_AMBIENT,       "surface.ambient",      0.3_f,
        "Relative intensity of an ambient light, illuminating all shaded points." },
    { GuiSettingsId::SURFACE_EMISSION,      "surface.emission",     1._f,
        "Emission multiplier used by raytracer. Note that emission is only enabled for Beauty quantity." },
    { GuiSettingsId::RAYTRACE_SUBSAMPLING,  "raytrace.subsampling", 1,
        "Specifies a number of subsampled iterations of the progressive renderer. Larger values speed up the "
        "start-up of the render at a cost of lower resolution of the render." },
    { GuiSettingsId::RAYTRACE_ITERATION_LIMIT, "raytrace.iteration_limit", 10,
        "Number of iterations of the render, including the subsampled iterations. " },
    { GuiSettingsId::RAYTRACE_HDRI,         "raytrace.hdri",        std::string(""),
        "Optional spherical bitmap used as an environment. Empty means the environment is black." },
    { GuiSettingsId::RAYTRACE_BRDF,         "raytrace.brdf",        BrdfEnum::LAMBERT,
        "Surface BRDF. Applicable for raytracer. "},
    { GuiSettingsId::RAYTRACE_SHADOWS,      "raytrace.shadows",     true,
        "Take into account occlusions when computing surface illumination." },
    { GuiSettingsId::RAYTRACE_SPHERES,      "raytrace.spheres",     false,
        "If true, raytraced surface is given by spheres centered at particles, "
        "otherwise isosurface of a colorfield is rendered." },
    { GuiSettingsId::VOLUME_EMISSION,       "volume.emission",      1.e-3_f,
        "Volume emission per unit length. Used by volumetric renderer." },
    { GuiSettingsId::VOLUME_ABSORPTION,     "volume.absorption",    0._f,
        "Absorption per unit length. Used by volumetric renderer." },
    { GuiSettingsId::RENDER_GHOST_PARTICLES, "render_ghost_particles", true,
        "If true, ghost particles will be displayed as transparent circles, otherwise they are hidden." },
    { GuiSettingsId::BACKGROUND_COLOR,      "background_color",     Vector(0._f, 0._f, 0._f, 1._f),
        "Background color of the rendered image." },
    { GuiSettingsId::COLORMAP_TYPE,         "colormap.type",        ColorMapEnum::LINEAR,
        "Color mapping applied on the rendered image." },
    { GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR, "colormap.logarithmic.factor",  2._f,
        "Compression factor used by the logarithmic colormapper. Higher values imply stronger compression of "
        "intensive pixels. Low values (~0.01) effectively produce a linear colormapping." },
    { GuiSettingsId::REDUCE_LOWFREQUENCY_NOISE, "reduce_lowfrequency_noise", false,
        "Reduces the low-frequency noise ('splotches') in the render." },
    { GuiSettingsId::SHOW_KEY,              "show_key",             true,
        "Include a color pallete and a distance scale in the rendered image." },
    { GuiSettingsId::FORCE_GRAYSCALE,       "force_grayscale",      false,
        "Palette used for particle colorization is converted to grayscale. Useful for checking how the "
        "image will look when printed on blank-and-white printer. "},
    { GuiSettingsId::ANTIALIASED,           "antialiased",          false,
        "Draw particles with antialiasing. Improves quality of the image, but may slow down the rendering." },
    { GuiSettingsId::SMOOTH_PARTICLES,      "smooth_particles",     false,
        "If true, rendered particles will be smoothed using cubic spline kernel. Useful to visualize the actual "
        "extend of particles."},
    { GuiSettingsId::CONTOUR_SPACING,       "contour.spacing",      10._f,
        "Different between values corresponding to subsequenct iso-lines" },
    { GuiSettingsId::CONTOUR_GRID_SIZE,     "contour.grid_size",    100,
        "TODO" },
    { GuiSettingsId::CONTOUR_SHOW_LABELS,   "contour.show_labels",  true,
        "TODO" },
    { GuiSettingsId::DEFAULT_COLORIZER,     "default_colorizer",    ColorizerId::VELOCITY,
        "Default colorizer shown when the simulation starts." },
    { GuiSettingsId::DEFAULT_PANES,         "default_panes",
      PaneEnum::RENDER_PARAMS | PaneEnum::PALETTE | PaneEnum::PARTICLE_DATA | PaneEnum::PLOTS  | PaneEnum::STATS,
        "Default panes in the run page." },

    /// Window settings
    { GuiSettingsId::WINDOW_TITLE,          "window.title",         std::string("OpenSPH"),
        "Title of the main window of the application." },
    { GuiSettingsId::WINDOW_WIDTH,          "window.width",         1110,
        "Width of the main window." },
    { GuiSettingsId::WINDOW_HEIGHT,         "window.height",        600,
        "Height of the main window." },
    { GuiSettingsId::PLOT_INTEGRALS,        "plot.integrals",       PlotEnum::ALL,
        "Integrals to compute and plot during the simulation. Can be one or more values of the following:\n"
        + EnumMap::getDesc<PlotEnum>() },
    { GuiSettingsId::PLOT_INITIAL_PERIOD,   "plot.initial_period",  0.1_f,
        "Initial period of time-dependent plots." },
    { GuiSettingsId::PLOT_OVERPLOT_SFD,     "plot.overplot_sfd",    std::string(""),
        "Path to the file containing SFD to plot over the computed one. The file must contain lines with value "
        "N(>D) and D [km]. If empty, no SFD is drawn."},
});
// clang-format on

/// \todo do we really need to specialize the function here to avoid linker error?
template <>
const Settings<GuiSettingsId>& Settings<GuiSettingsId>::getDefaults() {
    SPH_ASSERT(instance != nullptr);
    return *instance;
}

// Explicit instantiation
template class Settings<GuiSettingsId>;
template class SettingsIterator<GuiSettingsId>;

NAMESPACE_SPH_END
