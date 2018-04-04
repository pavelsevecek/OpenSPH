#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template<>
AutoPtr<RunSettings> RunSettings::instance (new RunSettings {
    /// Parameters of the run
    { RunSettingsId::RUN_NAME,                      "run.name",                 std::string("unnamed run") },
    { RunSettingsId::RUN_COMMENT,                   "run.comment",              std::string("") },
    { RunSettingsId::RUN_AUTHOR,                    "run.author",               std::string("Pavel Sevecek") },
    { RunSettingsId::RUN_EMAIL,                     "run.email",                std::string("sevecek@sirrah.troja.mff.cuni.cz") },
    { RunSettingsId::RUN_OUTPUT_TYPE,               "run.output.type",          int(OutputEnum::TEXT_FILE) },
    { RunSettingsId::RUN_OUTPUT_INTERVAL,           "run.output.interval",      0.1_f },
    { RunSettingsId::RUN_OUTPUT_NAME,               "run.output.name",          std::string("out_%d.txt") },
    { RunSettingsId::RUN_OUTPUT_PATH,               "run.output.path",          std::string("out") },
    { RunSettingsId::RUN_THREAD_CNT,                "run.thread.cnt",           0 },
    { RunSettingsId::RUN_THREAD_GRANULARITY,        "run.thread.granularity",   1000 },
    { RunSettingsId::RUN_LOGGER,                    "run.logger",               int(LoggerEnum::STD_OUT) },
    { RunSettingsId::RUN_LOGGER_FILE,               "run.logger.file",          std::string("log.txt") },
    { RunSettingsId::RUN_STATISTICS_STEP,           "run.statistics_step",      100 },
    { RunSettingsId::RUN_TIME_RANGE,                "run.time_range",           Interval(0._f, 10._f) },
    { RunSettingsId::RUN_TIMESTEP_CNT,              "run.timestep_cnt",         0 },
    { RunSettingsId::RUN_WALLCLOCK_TIME,            "run.wallclock_time",       0._f },
    { RunSettingsId::RUN_RNG,                       "run.rng",                  int(RngEnum::BENZ_ASPHAUG) },
    { RunSettingsId::RUN_RNG_SEED,                  "run.rng.seed",             1234 },

    /// SPH solvers
    { RunSettingsId::SOLVER_TYPE,                   "solver.type",                      int(SolverEnum::SYMMETRIC_SOLVER) },
    { RunSettingsId::SOLVER_FORCES,                 "solver.forces",                    int(ForceEnum::PRESSURE_GRADIENT) | int(ForceEnum::SOLID_STRESS) },
    { RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH,     "solver.adaptive_smoothing_length", int(SmoothingLengthEnum::CONTINUITY_EQUATION) },
    { RunSettingsId::SUMMATION_DENSITY_DELTA,       "solver.summation.density_delta",   1.e-3_f },
    { RunSettingsId::SUMMATION_MAX_ITERATIONS,      "solver.summation.max_iterations",  5 },
    { RunSettingsId::XSPH_EPSILON,                  "solver.xsph.epsilon",              1._f },

    /// Global SPH parameters
    { RunSettingsId::SPH_KERNEL,                    "sph.kernel",               int(KernelEnum::CUBIC_SPLINE) },
    { RunSettingsId::SPH_KERNEL_ETA,                "sph.kernel.eta",           1.5_f },
    { RunSettingsId::SPH_NEIGHBOUR_RANGE,           "sph.neighbour.range",      Interval(25._f, 100._f) },
    { RunSettingsId::SPH_NEIGHBOUR_ENFORCING,       "sph.neighbour.enforcing",  0.2_f },
    { RunSettingsId::SPH_AV_ALPHA,                  "sph.av.alpha",             1.5_f },
    { RunSettingsId::SPH_AV_BETA,                   "sph.av.beta",              3._f },
    { RunSettingsId::SPH_AV_TYPE,                   "sph.av.type",              int(ArtificialViscosityEnum::STANDARD) },
    { RunSettingsId::SPH_AV_BALSARA,                "sph.av.balsara",           false },
    { RunSettingsId::SPH_AV_BALSARA_STORE,          "sph.av.balsara.store",     false },
    { RunSettingsId::SPH_AV_STRESS_EXPONENT,        "sph.artificial_stress_exponent",   4._f },
    { RunSettingsId::SPH_AV_STRESS_FACTOR,          "sph.artificial_stress_factor",     0.15_f },
    { RunSettingsId::SPH_SMOOTHING_LENGTH_MIN,      "sph.smoothing_length.min",         1e-5_f },
    { RunSettingsId::SPH_PARTICLE_ROTATION,         "sph.particle_rotation",            false },
    { RunSettingsId::SPH_PHASE_ANGLE,               "sph.phase_angle",                  false },
    { RunSettingsId::SPH_FINDER,                    "sph.finder",                       int(FinderEnum::UNIFORM_GRID) },
    { RunSettingsId::SPH_FINDER_COMPACT_THRESHOLD,  "sph.finder.compact_threshold",     0.5_f },
    { RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, "sph.correction_tensor",        false },
    { RunSettingsId::SPH_SUM_ONLY_UNDAMAGED,            "sph.sum_only_undamaged",       true },
    { RunSettingsId::SPH_FORMULATION,                   "sph.formulation",              int(FormulationEnum::STANDARD) },
    { RunSettingsId::SPH_STABILIZATION_DAMPING,         "sph.stabilization_damping",    0.1_f },

    /// Global parameters of N-body simulations
    { RunSettingsId::NBODY_INERTIA_TENSOR,          "nbody.inertia_tensor",     true },
    { RunSettingsId::NBODY_MAX_ROTATION_ANGLE,      "nbody.max_rotation_angle", 0.5_f },

    /// Gravity
    { RunSettingsId::GRAVITY_SOLVER,                "gravity.solver",           int(GravityEnum::BARNES_HUT) },
    { RunSettingsId::GRAVITY_OPENING_ANGLE,         "gravity.opening_angle",    0.5_f },
    { RunSettingsId::GRAVITY_LEAF_SIZE,             "gravity.leaf_size",        25 },
    { RunSettingsId::GRAVITY_MULTIPOLE_ORDER,       "gravity.multipole_order",  3 },
    { RunSettingsId::GRAVITY_KERNEL,                "gravity.kernel",           int(GravityKernelEnum::SPH_KERNEL) },

    /// Collision handling
    { RunSettingsId::COLLISION_HANDLER,             "collision.handler",                int(CollisionHandlerEnum::MERGE_OR_BOUNCE) },
    { RunSettingsId::COLLISION_OVERLAP,             "collision.overlap",                int(OverlapEnum::REPEL) },
    { RunSettingsId::COLLISION_RESTITUTION_NORMAL,  "collision.restitution_normal",     0.8_f },
    { RunSettingsId::COLLISION_RESTITUTION_TANGENT, "collision.restitution_tangent",    1.0_f },
    { RunSettingsId::COLLISION_ALLOWED_OVERLAP,     "collision.allowed_overlap",        0.01_f },
    { RunSettingsId::COLLISION_MERGING_LIMIT,       "collision.merging_limit",          1._f },

    /// Timestepping parameters
    { RunSettingsId::TIMESTEPPING_INTEGRATOR,       "timestep.integrator",      int(TimesteppingEnum::PREDICTOR_CORRECTOR) },
    { RunSettingsId::TIMESTEPPING_COURANT_NUMBER,   "timestep.courant_number",         1._f },
    { RunSettingsId::TIMESTEPPING_COURANT_NEIGHBOUR_LIMIT, "timestep.courant_neighbour_limit",  1 },
    { RunSettingsId::TIMESTEPPING_MAX_TIMESTEP,     "timestep.max_step",        0.1_f /*s*/}, /// \todo units necessary in settings!!!
    { RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, "timestep.initial",         0.03_f },
    { RunSettingsId::TIMESTEPPING_CRITERION,        "timestep.criterion",       int(TimeStepCriterionEnum::ALL) },
    { RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR,  "timestep.adaptive.factor", 0.2_f },
    { RunSettingsId::TIMESTEPPING_MEAN_POWER,       "timestep.mean_power",      -INFTY },
    { RunSettingsId::TIMESTEPPING_MAX_CHANGE,       "timestep.max_change",      INFTY },

    /// Selected coordinate system, rotation of bodies
    { RunSettingsId::FRAME_ANGULAR_FREQUENCY,       "frame.angular_frequency",  Vector(0._f) },

    /// Computational domain and boundary conditions
    { RunSettingsId::DOMAIN_TYPE,                   "domain.type",              int(DomainEnum::NONE) },
    { RunSettingsId::DOMAIN_BOUNDARY,               "domain.boundary",          int(BoundaryEnum::NONE) },
    { RunSettingsId::DOMAIN_GHOST_MIN_DIST,         "domain.ghosts.min_dist",   0.1_f },
    { RunSettingsId::DOMAIN_FROZEN_DIST,            "domain.frozen_dist",       2.5_f }, /// \todo this should really be the kernel radius
    { RunSettingsId::DOMAIN_CENTER,                 "domain.center",            Vector(0._f) },
    { RunSettingsId::DOMAIN_RADIUS,                 "domain.radius",            1._f },
    { RunSettingsId::DOMAIN_HEIGHT,                 "domain.height",            1._f },
    { RunSettingsId::DOMAIN_SIZE,                   "domain.size",              Vector(1._f) },
    { RunSettingsId::BOUNDARY_THRESHOLD,            "boundary.threshold",       40 },

    /// Output of results
    { RunSettingsId::OUTPUT_SAVE_INITIAL_POSITION,  "output.save_initial_positions", false }
});
// clang-format on


// clang-format off
template<>
AutoPtr<BodySettings> BodySettings::instance (new BodySettings {
    /// Equation of state
    { BodySettingsId::EOS,                     "eos",                          int(EosEnum::IDEAL_GAS) },
    { BodySettingsId::ADIABATIC_INDEX,         "eos.adiabatic_index",          1.4_f },
    { BodySettingsId::TAIT_GAMMA,              "eos.tait.gamma",               7._f },
    { BodySettingsId::TAIT_SOUND_SPEED,        "eos.tait.sound_speed",         1484._f }, // value for water
    { BodySettingsId::TILLOTSON_SMALL_A,       "eos.tillotson.small_a",        0.5_f },
    { BodySettingsId::TILLOTSON_SMALL_B,       "eos.tillotson.small_b",        1.5_f },
    { BodySettingsId::TILLOTSON_ALPHA,         "eos.tillotson.alpha",          5._f },
    { BodySettingsId::TILLOTSON_BETA,          "eos.tillotson.beta",           5._f },
    { BodySettingsId::TILLOTSON_NONLINEAR_B,   "eos.tillotson.nonlinear_b",    2.67e10_f },
    { BodySettingsId::TILLOTSON_SUBLIMATION,   "eos.tillotson.sublimation",    4.87e8_f },
    { BodySettingsId::TILLOTSON_ENERGY_IV,     "eos.tillotson.energy_iv",      4.72e6_f },
    { BodySettingsId::TILLOTSON_ENERGY_CV,     "eos.tillotson.energy_cv",      1.82e7_f },
    { BodySettingsId::GRUNEISEN_GAMMA,         "eos.mie_gruneisen.gamma",      2._f },      // value for copper taken from wikipedia
    { BodySettingsId::HUGONIOT_SLOPE,          "eos.mie_gruneises.hugoniot_slope",   1.5_f }, // value for copper taken from wikipedia
    { BodySettingsId::BULK_SOUND_SPEED,        "eos.mie_gruneises.bulk_sound_speed", 3933._f }, // value for copper taken from wikipedia

    /// Yielding & Damage
    { BodySettingsId::RHEOLOGY_YIELDING,    "rheology.yielding",            int(YieldingEnum::VON_MISES) },
    { BodySettingsId::RHEOLOGY_DAMAGE,      "rheology.damage",              int(FractureEnum::NONE) },
    { BodySettingsId::ELASTICITY_LIMIT,     "rheology.elasticity_limit",    3.5e9_f },
    { BodySettingsId::MELT_ENERGY,          "rheology.melt_energy",         3.4e6_f },
    { BodySettingsId::COHESION,             "rheology.cohesion",            9.e7_f },
    { BodySettingsId::INTERNAL_FRICTION,    "rheology.internal_friction",   2._f },
    { BodySettingsId::DRY_FRICTION,         "rheology.dry_friction",        0.8_f },
    { BodySettingsId::BRITTLE_DUCTILE_TRANSITION_PRESSURE, "rheology.brittle_ductile_transition_pressure", 1.23e9_f },  // Modeling damage and deformation in impact simulations
    { BodySettingsId::BRITTLE_PLASTIC_TRANSITION_PRESSURE, "rheology.brittle_plastic_transition_pressure", 2.35e9_f },  // Collions et al. (2004)
    { BodySettingsId::MOHR_COULOMB_STRESS,  "rheology.mohr_coulomb_stress", 0._f },
    { BodySettingsId::FRICTION_ANGLE,       "rheology.friction_angle",      0._f },

    /// Material properties
    { BodySettingsId::DENSITY,                 "material.density",             2700._f },
    { BodySettingsId::DENSITY_RANGE,           "material.density.range",       Interval(10._f, INFTY) },
    { BodySettingsId::DENSITY_MIN,             "material.density.min",         50._f },
    { BodySettingsId::ENERGY,                  "material.energy",              0._f },
    { BodySettingsId::ENERGY_RANGE,            "material.energy.range",        Interval(0._f, INFTY) },
    { BodySettingsId::ENERGY_MIN,              "material.energy.min",          1._f },
    { BodySettingsId::DAMAGE,                  "material.damage",              0._f },
    { BodySettingsId::DAMAGE_RANGE,            "material.damage.range",        Interval(0.f, 1._f) },
    { BodySettingsId::DAMAGE_MIN,              "material.damage.min",          0.03_f },
    { BodySettingsId::STRESS_TENSOR,           "material.stress_tensor",       TracelessTensor(0._f) },
    { BodySettingsId::STRESS_TENSOR_MIN,       "material.stress_tensor.min",   1.e5_f },
    { BodySettingsId::BULK_MODULUS,            "material.bulk_modulus",        2.67e10_f },
    { BodySettingsId::SHEAR_MODULUS,           "material.shear_modulus",       2.27e10_f },
    { BodySettingsId::YOUNG_MODULUS,           "material.young_modulus",       5.7e10_f },
    { BodySettingsId::ELASTIC_MODULUS,         "material.elastic_modulus",     8.e9_f }, /// \todo use the value of Basalt, this one is for quartz from Wiki
    { BodySettingsId::RAYLEIGH_SOUND_SPEED,    "material.rayleigh_speed",      0.4_f },
    { BodySettingsId::WEIBULL_COEFFICIENT,     "material.weibull_coefficient", 4.e35_f },
    { BodySettingsId::WEIBULL_EXPONENT,        "material.weibull_exponent",    9._f },
    { BodySettingsId::BULK_VISCOSITY,          "material.bulk_viscosity",      1.e20_f },
    { BodySettingsId::SHEAR_VISCOSITY,         "material.shear_viscosity",     1.e20_f },
    { BodySettingsId::DIFFUSIVITY,             "material.diffusivity",         1._f },
    { BodySettingsId::SURFACE_TENSION,         "material.surface_tension",     1._f },

    /// SPH parameters specific for the body
    { BodySettingsId::INITIAL_DISTRIBUTION,    "sph.initial_distribution",     int(DistributionEnum::HEXAGONAL) },
    { BodySettingsId::CENTER_PARTICLES,        "sph.center_particles",         true },
    { BodySettingsId::PARTICLE_SORTING,        "sph.particle_sorting",         false },
    { BodySettingsId::DISTRIBUTE_MODE_SPH5,    "sph.distribute_mode_sph5",     false },
    { BodySettingsId::DIELH_STRENGTH,          "sph.diehl_strength",           0.1_f },
    { BodySettingsId::DIEHL_MAX_DIFFERENCE,    "sph.diehl_max_difference",     10 },
    { BodySettingsId::PARTICLE_COUNT,          "sph.particle_count",           10000 },
    { BodySettingsId::MIN_PARTICLE_COUNT,      "sph.min_particle_count",       100 },
    { BodySettingsId::AV_ALPHA,                "av.alpha",                     1.5_f },
    { BodySettingsId::AV_ALPHA_RANGE,          "av.alpha.range",               Interval(0.05_f, 1.5_f) },
    { BodySettingsId::AV_BETA,                 "av.beta",                      3._f },
    { BodySettingsId::AV_ALPHA_RANGE,          "av.beta.range",                Interval(0.1_f, 3._f) },
    { BodySettingsId::BODY_CENTER,             "body.center",                  Vector(0._f) },
    { BodySettingsId::BODY_VELOCITY,           "body.velocity",                Vector(0._f) },
    { BodySettingsId::BODY_ANGULAR_VELOCITY,   "body.angular_velocity",        Vector(0._f) },
});
// clang-format on

NAMESPACE_SPH_END
