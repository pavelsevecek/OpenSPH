#include "gui/Settings.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template<>
AutoPtr<GuiSettings> GuiSettings::instance (new GuiSettings {
    /// Camera pameters
    { GuiSettingsId::CAMERA,                "camera",               CameraEnum::ORTHO,
        "Specifies the projection of the particles to the image. Can be one of the following:\n" + EnumMap::getDesc<CameraEnum>() },
    { GuiSettingsId::PARTICLE_RADIUS,       "particle_radius",      0.5_f,
        "Multiplier of the particle radius for drawing." },
    { GuiSettingsId::ORTHO_CUTOFF,          "ortho.cutoff",         0.1_f,
        "Cut-off distance from center plane. Particles further away are not drawn. Used by particle renderer." },
    { GuiSettingsId::ORTHO_ZOFFSET,         "ortho.zoffset",        0._f,
        "Distance of the orthographic camera from the center plane. Used by raytracer to get the ray origin. "},
    { GuiSettingsId::ORTHO_PROJECTION,      "ortho.projection",     OrthoEnum::XY,
        "Obsolete" },
    { GuiSettingsId::PERSPECTIVE_FOV,       "perspective.fov",      PI / 3._f,
        "Field of view of the perspective camera (in radians)." },
    { GuiSettingsId::PERSPECTIVE_POSITION,  "perspective.position", Vector(0._f, 0._f, 1._f),
        "Position of the perspective camera in space." },
    { GuiSettingsId::PERSPECTIVE_TARGET,    "perspective.target",   Vector(0._f),
        "Look-at point of the perspective camera. Actual distance from the camera does not matter." },
    { GuiSettingsId::PERSPECTIVE_UP,        "perspective.up",       Vector(0._f, 1._f, 0._f),
        "Up-vector of the perspective camera. Does not have to be normalized." },

    /// Particle visualization
    { GuiSettingsId::RENDERER,              "renderer",             RendererEnum::PARTICLE,
        "Selected renderer for particle visualization. Can be one of the following:\n" + EnumMap::getDesc<RendererEnum>() },
    { GuiSettingsId::VIEW_WIDTH,            "view.width",           800,
        "Width of the rendered image (in pixels)." },
    { GuiSettingsId::VIEW_HEIGHT,           "view.height",          600,
        "Height of the rendered image (in pixels)." },
    { GuiSettingsId::VIEW_MAX_FRAMERATE,    "view.max_framerate",   100, // ms
        "Minimal refresh period of the drawed bitmap. Used to avoid visualization unnecessarily affecting "
        "the performance of the simulation." },
    { GuiSettingsId::VIEW_GRID_SIZE,        "view.grid_size",       0._f,
        "Step of the grid drawn into the bitmap. If zero, no grid is drawn." },
    { GuiSettingsId::SURFACE_RESOLUTION,    "surface.resolution",   100._f,  // m
        "Resolution of the meshed surface (in world units). Lower values means the mesh is more detailed, "
        "but construction takes (significantly) more time and memory." },
    { GuiSettingsId::SURFACE_LEVEL,         "surface.level",        0.3_f,
        "Surface level for mesh renderer and raytracer. Specifies the value of the constructed/intersected "
        "iso-surface of color field." },
    { GuiSettingsId::SURFACE_SUN_POSITION,  "surface.sun_position", Vector(0.f, 0.f, 1.f),
        "Direction to the sun, used for shading in mesh renderer in raytracer." },
    { GuiSettingsId::SURFACE_SUN_INTENSITY, "surface.sun_intentity", 0.7_f,
        "Relative intensity of the sun, used for shading in mesh renderer in raytracer." },
    { GuiSettingsId::SURFACE_AMBIENT,       "surface.ambient",      0.3_f,
        "Relative intensity of an ambient light, illuminating all shaded points." },
    { GuiSettingsId::ORTHO_VIEW_CENTER,     "ortho.center",          Vector(0._f),
        "Center point of the orthographic camera." },
    { GuiSettingsId::ORTHO_FOV,             "ortho.fov",             0._f,
        "Field of view of the orthographic camera. Specified as distance (not an angle). Value 0 means the "
        "field of view is computed automatically to fit all particles inside the view."},
    { GuiSettingsId::RAYTRACE_SUBSAMPLING,  "raytrace.subsampling", 1,
        "Specifies a step in pixels between the two pixels computed by raytracing. Pixels between the computed "
        "ones are linearly interpolated. Larger value speeds up the rendering at a cost of lower effective "
        "resolution of the rendered image." },
    { GuiSettingsId::RAYTRACE_HDRI,         "raytrace.hdri",        std::string(""),
        "Optional spherical bitmap used as an environment. Empty means the environment is black." },
    { GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY,      "raytrace.texture_primary",     std::string(""),
        "Optional bitmap used as a texture for the primary body (target). Applicable for raytracer." },
    { GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY,    "raytrace.texture_secondary",   std::string(""),
        "Optional bitmap used as a texture for the secondary body (impactor). Applicable for raytracer." },

    /// Window settings
    { GuiSettingsId::WINDOW_TITLE,          "window.title",         std::string("SPH"),
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

    /// Saved animation frames
    { GuiSettingsId::IMAGES_RENDERER,       "images.renderer",      RendererEnum::PARTICLE,
        "Type of the renderer used when creating snapshot images. Values are the same as for 'renderer' entry." },
    { GuiSettingsId::IMAGES_SAVE,           "images.save",          false,
        "If true, snapshot images are periodically saved to disk." },
    { GuiSettingsId::IMAGES_PATH,           "images.path",          std::string("imgs/"),
        "Directory where the images are saved." },
    { GuiSettingsId::IMAGES_NAME,           "images.name",          std::string("img_%e_%d.png"),
        "File mask of the created images. Can contain wildcard %d, replaced with the counter of an image, "
        "and %e, replaced with the name of the renderer quantity." },
    { GuiSettingsId::IMAGES_MAKE_MOVIE,     "images.make_movie",    false,
        "If true, an animation is made from the saved images automatically at the end of the simulation. "
        "Requires ffmpeg to be installed." },
    { GuiSettingsId::IMAGES_MOVIE_NAME,     "images.movie_name",    std::string("%e.avi"),
        "File mask of the animation." },
    { GuiSettingsId::IMAGES_TIMESTEP,       "images.time_step",     0.1_f,
        "Period of creating the snapshot images." },
    { GuiSettingsId::IMAGES_WIDTH,          "images.width",         800,
        "Width of the created images." },
    { GuiSettingsId::IMAGES_HEIGHT,         "images.height",        600,
        "Height of the created images." },

    /// Color palettes
    { GuiSettingsId::PALETTE_DENSITY,       "palette.density",      Interval(2650._f, 2750._f) },
    { GuiSettingsId::PALETTE_MASS,          "palette.mass",         Interval(0._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_VELOCITY,      "palette.velocity",     Interval(0._f, 1._f) },
    { GuiSettingsId::PALETTE_ACCELERATION,  "palette.acceleration", Interval(0.1_f, 100._f) },
    { GuiSettingsId::PALETTE_PRESSURE,      "palette.pressure",     Interval(-1000._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_ENERGY,        "palette.energy",       Interval(1._f, 1.e6_f) },
    { GuiSettingsId::PALETTE_STRESS,        "palette.stress",       Interval(0._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_DAMAGE,        "palette.damage",       Interval(0._f, 1._f) },
    { GuiSettingsId::PALETTE_DIVV,          "palette.divv",         Interval(-0.1_f, 0.1_f) },
    { GuiSettingsId::PALETTE_GRADV,         "palette.gradv",        Interval(0._f, 1.e-3_f) },
    { GuiSettingsId::PALETTE_ROTV,          "palette.rotv",         Interval(0._f, 4._f) },
    { GuiSettingsId::PALETTE_RADIUS,        "palette.radius",       Interval(0._f, 1.e3_f) },
    { GuiSettingsId::PALETTE_TOTAL_ENERGY,  "palette.total_energy", Interval(1.e14_f, 1.e17_f) },
    { GuiSettingsId::PALETTE_DENSITY_PERTURB,               "palette.density_perturb",                  Interval(-1.e-6_f, 1.e-6_f) },
    { GuiSettingsId::PALETTE_ANGULAR_VELOCITY,              "palette.angular_velocity",                 Interval(0._f, 1.e-3_f) },
    { GuiSettingsId::PALETTE_MOMENT_OF_INERTIA,             "palette.moment_of_inertia",                Interval(0._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_STRAIN_RATE_CORRECTION_TENSOR, "palette.strain_rate_correction_tensor",    Interval(0.5_f, 5._f) },
    { GuiSettingsId::PALETTE_ACTIVATION_STRAIN,             "palette.activation_strain",                Interval(2.e-4_f, 8.e-4_f) },
    { GuiSettingsId::PALETTE_VELOCITY_SECOND_DERIVATIVES,   "palette.velocity_second_derivatives",      Interval(0._f, 5._f) },
});
// clang-format on

/// \todo do we really need to specialize the function here to avoid linker error?
template <>
const Settings<GuiSettingsId>& Settings<GuiSettingsId>::getDefaults() {
    ASSERT(instance != nullptr);
    return *instance;
}

// Explicit instantiation
template class Settings<GuiSettingsId>;

NAMESPACE_SPH_END
