#include "gui/Settings.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template<>
AutoPtr<GuiSettings> GuiSettings::instance (new GuiSettings {
    /// Renderer
    { GuiSettingsId::RENDERER,         "renderer",         int(RendererEnum::PARTICLE) },
    { GuiSettingsId::RENDER_WIDTH,     "render.width",     800 },
    { GuiSettingsId::RENDER_HEIGHT,    "render.height",    600 },

    /// Camera pameters
    { GuiSettingsId::CAMERA,                "camera",               int(CameraEnum::ORTHO) },
    { GuiSettingsId::PARTICLE_RADIUS,       "particle_radius",      0.5_f },
    { GuiSettingsId::ORTHO_CUTOFF,          "ortho.cutoff",         0.1_f },
    { GuiSettingsId::ORTHO_ZOFFSET,         "ortho.zoffset",        0._f },
    { GuiSettingsId::ORTHO_PROJECTION,      "ortho.projection",     int(OrthoEnum::XY) },
    { GuiSettingsId::PERSPECTIVE_FOV,       "perspective.fov",      PI / 3._f },
    { GuiSettingsId::PERSPECTIVE_POSITION,  "perspective.position", Vector(0._f, 0._f, 1._f) },
    { GuiSettingsId::PERSPECTIVE_TARGET,    "perspective.target",   Vector(0._f) },
    { GuiSettingsId::PERSPECTIVE_UP,        "perspective.up",       Vector(0._f, 1._f, 0._f) },

    /// Particle visualization
    { GuiSettingsId::SURFACE_RESOLUTION,    "surface.resolution",   100._f },  // m
    { GuiSettingsId::SURFACE_LEVEL,         "surface.level",        0.3_f },
    { GuiSettingsId::SURFACE_SUN_POSITION,  "surface.sun_position", Vector(0.f, 0.f, 1.f) },
    { GuiSettingsId::SURFACE_SUN_INTENSITY, "surface.sun_intentity", 0.7_f },
    { GuiSettingsId::SURFACE_AMBIENT,       "surface.ambient",      0.3_f },
    { GuiSettingsId::ORTHO_VIEW_CENTER,     "view.center",          Vector(0._f) },
    { GuiSettingsId::ORTHO_FOV,             "view.fov",             1._f },
    { GuiSettingsId::VIEW_WIDTH,            "view.width",           800 },
    { GuiSettingsId::VIEW_HEIGHT,           "view.height",          600 },
    { GuiSettingsId::VIEW_MAX_FRAMERATE,    "view.max_framerate",   100 }, // ms
    { GuiSettingsId::VIEW_GRID_SIZE,        "view.grid_size",       0._f },

    /// Window settings
    { GuiSettingsId::WINDOW_TITLE,     "window.title",     std::string("SPH") },
    { GuiSettingsId::WINDOW_WIDTH,     "window.width",     1110 },
    { GuiSettingsId::WINDOW_HEIGHT,    "window.height",    600 },
    { GuiSettingsId::PLOT_INTEGRALS,   "plot.integrals",   int(PlotEnum::ALL) },

    /// Saved animation frames
    { GuiSettingsId::IMAGES_RENDERER,       "images.renderer",      int(RendererEnum::PARTICLE) },
    { GuiSettingsId::IMAGES_SAVE,           "images.save",          false },
    { GuiSettingsId::IMAGES_PATH,           "images.path",          std::string("imgs/") },
    { GuiSettingsId::IMAGES_NAME,           "images.name",          std::string("img_%e_%d.png") },
    { GuiSettingsId::IMAGES_MAKE_MOVIE,     "images.make_movie",    false },
    { GuiSettingsId::IMAGES_MOVIE_NAME,     "images.movie_name",    std::string("%e.avi") },
    { GuiSettingsId::IMAGES_TIMESTEP,       "images.time_step",     0.1_f },
    { GuiSettingsId::IMAGES_WIDTH,          "images.width",         800 },
    { GuiSettingsId::IMAGES_HEIGHT,         "images.height",        600 },

    /// Color palettes
    { GuiSettingsId::PALETTE_DENSITY,       "palette.density",      Interval(2650._f, 2750._f) },
    { GuiSettingsId::PALETTE_MASS,          "palette.mass",         Interval(0._f, 1.e10_f) },
    { GuiSettingsId::PALETTE_VELOCITY,      "palette.velocity",     Interval(0._f, 1._f) },
    { GuiSettingsId::PALETTE_ACCELERATION,  "palette.acceleration", Interval(0._f, 100._f) },
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
