#include "io/Output.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

static auto DEFAULT_QUANTITY_IDS = OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY |
                                   OutputQuantityFlag::SMOOTHING_LENGTH | OutputQuantityFlag::MASS |
                                   OutputQuantityFlag::DENSITY | OutputQuantityFlag::PRESSURE |
                                   OutputQuantityFlag::ENERGY | OutputQuantityFlag::DEVIATORIC_STRESS;


static RegisterEnum<OutputQuantityFlag> sQuantity({
    { OutputQuantityFlag::POSITION, "position", "Positions of particles, always a vector quantity." },
    { OutputQuantityFlag::SMOOTHING_LENGTH, "smoothing_length", "Smoothing lenghts of particles." },
    { OutputQuantityFlag::VELOCITY, "velocity", "Velocities of particles, always a vector quantity." },
    { OutputQuantityFlag::MASS, "mass", "Particle masses, always a scalar quantity." },
    { OutputQuantityFlag::PRESSURE,
        "pressure",
        "Pressure, reduced by yielding and fracture model (multiplied by 1-damage); always a scalar "
        "quantity." },
    { OutputQuantityFlag::DENSITY, "density", "Density, always a scalar quantity." },
    { OutputQuantityFlag::ENERGY, "energy", "Specific internal energy, always a scalar quantity." },
    // { QuantityId::SOUND_SPEED, "sound_speed", "Local sound speed, always a scalar quantity." },
    { OutputQuantityFlag::DEVIATORIC_STRESS,
        "deviatoric_stress",
        "Deviatoric stress tensor, always a traceless tensor stored in components xx, yy, xy, xz, yz." },
    { OutputQuantityFlag::DAMAGE, "damage", "Damage, reducing the pressure and deviatoric stress." },
    /* { QuantityId::VELOCITY_GRADIENT, "velocity_gradient", "Velocity gradient (strain rate)." },
     { QuantityId::VELOCITY_DIVERGENCE, "velocity_divergence", "Velocity divergence." },
     { QuantityId::VELOCITY_ROTATION, "velocity_rotation", "Velocity rotation (rotation rate)." },*/
    { OutputQuantityFlag::STRAIN_RATE_CORRECTION_TENSOR,
        "correction_tensor",
        "Symmetric tensor correcting kernel gradient for linear consistency." },
    { OutputQuantityFlag::MATERIAL_ID, "material_id", "ID of material, indexed from 0 to (#bodies - 1)." },
    { OutputQuantityFlag::INDEX, "index", "Index of particle, indexed from 0 to (#particles - 1)." },
});

// clang-format off
template<>
AutoPtr<RunSettings> RunSettings::instance (new RunSettings {
    { RunSettingsId::RUN_NAME,                     "run.name", std::string("unnamed run"),
        "User-specified name of the run. Can be stored in the metadata of output files." },
    { RunSettingsId::RUN_COMMENT,                   "run.comment", std::string(""),
        "Auxiliary comment of the run. Can be stored in the metadata of output files." },
    { RunSettingsId::RUN_AUTHOR,                    "run.author",               std::string("Pavel Sevecek"),
        "Name of the person performing this run." },
    { RunSettingsId::RUN_EMAIL,                     "run.email",                std::string("sevecek@sirrah.troja.mff.cuni.cz"),
        "E-mail of the run author." },
    { RunSettingsId::RUN_OUTPUT_TYPE,               "run.output.type",          OutputEnum::TEXT_FILE,
        "Format of the output files generated by the run. Can be one of the following:\n" + EnumMap::getDesc<OutputEnum>() },
    { RunSettingsId::RUN_OUTPUT_INTERVAL,           "run.output.interval",      0.1_f,
        "Interval of creating output files (in simulation time)." },
    { RunSettingsId::RUN_OUTPUT_NAME,               "run.output.name",          std::string("out_%d.txt"),
        "File mask of the created files. It can contain a wildcard %d, which is replaced by the output number and "
        "%t which is replaced by current simulation time." },
    { RunSettingsId::RUN_OUTPUT_PATH,               "run.output.path",          std::string("out"),
        "Directory where the output files are saved. Can be either absolute or relative path." },
    { RunSettingsId::RUN_OUTPUT_QUANTITIES, "run.output.quantitites", DEFAULT_QUANTITY_IDS,
        "List of quantities to write to text output. Applicable only for text output, binary output always stores "
        "all quantitites. Can be one or more values from:\n" + EnumMap::getDesc<OutputQuantityFlag>() },
    { RunSettingsId::RUN_THREAD_CNT,                "run.thread.cnt",           0,
        "Number of threads used by the simulation. 0 means all available threads are used." },
    { RunSettingsId::RUN_THREAD_GRANULARITY,        "run.thread.granularity",   1000,
        "Number of particles processed by one thread in a single batch. Lower number can help to distribute tasks "
        "between threads more evenly, higher number means faster processing of particles within single thread." },
    { RunSettingsId::RUN_LOGGER,                    "run.logger",               LoggerEnum::STD_OUT,
        "Type of a log generated by the simulation. Can be one of the following:\n" + EnumMap::getDesc<LoggerEnum>() },
    { RunSettingsId::RUN_LOGGER_FILE,               "run.logger.file",          std::string("log.txt"),
        "Specifies the path where the log is saved (if applicable)" },
    { RunSettingsId::RUN_TIME_RANGE,                "run.time_range",           Interval(0._f, 10._f),
        "Time interval of the simulation (in simulation time). The simulation can start in negative time, in which case "
        "no output files or log files are created until the time gets to positive values." },
    { RunSettingsId::RUN_TIMESTEP_CNT,              "run.timestep_cnt",         0,
        "Number of timesteps performed by the integrator. If zero, the criterion is not used. " },
    { RunSettingsId::RUN_WALLCLOCK_TIME,            "run.wallclock_time",       0._f,
        "Maximum wallclock time of the simulation. If zero, the criterion is not used. " },
    { RunSettingsId::RUN_RNG,                       "run.rng",                  RngEnum::BENZ_ASPHAUG,
        "Random number generator used by the simulation. Can be one of the following:\n" + EnumMap::getDesc<RngEnum>() },
    { RunSettingsId::RUN_RNG_SEED,                  "run.rng.seed",             1234,
        "Seed of the random number generator (if applicable)." },

    /// SPH solvers
    { RunSettingsId::SOLVER_TYPE,                   "solver.type",                      SolverEnum::SYMMETRIC_SOLVER,
        "Selected solver for computing derivatives of physical quantities. Can be one of the following:\n" + EnumMap::getDesc<SolverEnum>() },
    { RunSettingsId::SOLVER_FORCES,                 "solver.forces",                   ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS,
        "Forces included in the physical model of the simulation. Can be one or more values from: \n" + EnumMap::getDesc<ForceEnum>() },
    { RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH,     "solver.adaptive_smoothing_length", SmoothingLengthEnum::CONTINUITY_EQUATION,
        "Specifies how smoothing length is evolved in the simulation. Can be one or more values from: \n" + EnumMap::getDesc<SmoothingLengthEnum>() },
    { RunSettingsId::SUMMATION_DENSITY_DELTA,       "solver.summation.density_delta",   1.e-3_f,
        "Used by summation solver. Specifies the relative difference between densities in subsequenct iterations "
        "for which the iterative algorithm is terminated. Lower value means more precise evaluation of density "
        "at a cost of higher computation time. " },
    { RunSettingsId::SUMMATION_MAX_ITERATIONS,      "solver.summation.max_iterations",  5,
        "Used by summation solver. Specifies the maximum number of iterations for density computation." },
    { RunSettingsId::XSPH_EPSILON,                  "solver.xsph.epsilon",              1._f,
        "Epsilon parameter of XSph modification." },

    /// Global SPH parameters
    { RunSettingsId::SPH_KERNEL,                    "sph.kernel",               KernelEnum::CUBIC_SPLINE,
        "Type of the SPH kernel. Can be one of the following:\n" + EnumMap::getDesc<KernelEnum>() },
    { RunSettingsId::SPH_KERNEL_ETA,                "sph.kernel.eta",           1.5_f,
        "Multiplier of the kernel radius. Lower values means the particles are more localized (better spatial resolution), "
        "but they also have fewer neighbours, so the derivatives are evaluated with lower precision. Values between 1 and 2 "
        "should be used." },
    { RunSettingsId::SPH_NEIGHBOUR_RANGE,           "sph.neighbour.range",      Interval(25._f, 100._f),
        "Allowed numbers of particle neighbours. Applicable if neighbour enforcing is used for evolution of "
        "smoothing length. Note that even with this parameter set, it is not guaranteed that the number of "
        "neighbours will be within the interval for every particle, the code only tries to do so." },
    { RunSettingsId::SPH_NEIGHBOUR_ENFORCING,       "sph.neighbour.enforcing",  0.2_f,
        "'Strength' of the neighbour enforcing. The higher number means the derivative of the smoothing "
        "length can be higher, lower values means 'smoother' evolution of smooting length"},
    { RunSettingsId::SPH_AV_ALPHA,                  "sph.av.alpha",             1.5_f,
        "Coefficient alpha_AV of the standard artificial viscosity." },
    { RunSettingsId::SPH_AV_BETA,                   "sph.av.beta",              3._f,
        "Coefficient beta_AV of the standard artificial viscosity. "},
    { RunSettingsId::SPH_AV_TYPE,                   "sph.av.type",              ArtificialViscosityEnum::STANDARD,
        "Type of the artificial viscosity used by the SPH solver. Can be one of the following:\n" + EnumMap::getDesc<ArtificialViscosityEnum>() },
    { RunSettingsId::SPH_AV_BALSARA,                "sph.av.balsara",           false,
        "Specifies if the Balsara switch is used when computing artificial viscosity" },
    { RunSettingsId::SPH_AV_BALSARA_STORE,          "sph.av.balsara.store",     false,
        "Debug parameter; if true, Balsara coefficient is stored as a quantity and can be saved to output file. "},
    { RunSettingsId::SPH_AV_STRESS_EXPONENT,        "sph.artificial_stress_exponent",   4._f,
        "Kernel exponent of the artificial stress. "},
    { RunSettingsId::SPH_AV_STRESS_FACTOR,          "sph.artificial_stress_factor",     0.15_f,
        "Multiplicative factor of the artificial stress. "},
    { RunSettingsId::SPH_SMOOTHING_LENGTH_MIN,      "sph.smoothing_length.min",         1e-5_f,
        "Minimal value of the smoothing length (in meters). "},
    { RunSettingsId::SPH_PARTICLE_ROTATION,         "sph.particle_rotation",            false,
        "True if SPH particles should have an intrinsic angular momentum. Turns on a SPH modification "
        "with rotating SPH particles. EXPERIMENTAL!" },
    { RunSettingsId::SPH_PHASE_ANGLE,               "sph.phase_angle",                  false,
        "If true, phase angle of each particle is saved in storage and evolved in time. "},
    { RunSettingsId::SPH_FINDER,                    "sph.finder",                       FinderEnum::KD_TREE,
        "Acceleration structure used for finding neighbours (Kn queries). Can be one of the following:\n" + EnumMap::getDesc<FinderEnum>() },
    { RunSettingsId::SPH_FINDER_COMPACT_THRESHOLD,  "sph.finder.compact_threshold",     0.5_f,
        "Used by dynamic finder. Threshold value for switching between grid finder and K-d tree." },
    { RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, "sph.correction_tensor",        false,
        "If true, correction tensor is applied on gradient when computing strain rate. Essential "
        "for correct simulation of rotating bodies." },
    { RunSettingsId::SPH_SUM_ONLY_UNDAMAGED,            "sph.sum_only_undamaged",       true,
        "If true, completely damaged particles (D=1) are excluded when computing strain rate and "
        "stress divergence. Solver also excludes particles of different bodies; when computing "
        "strain rate in target, particles in impactor are excluded from the sum." },
    { RunSettingsId::SPH_FORMULATION,                   "sph.formulation",              FormulationEnum::STANDARD,
        "Specifies a discretization of SPH equations. Can be one of the following:\n" + EnumMap::getDesc<FormulationEnum>() },
    { RunSettingsId::SPH_STABILIZATION_DAMPING,         "sph.stabilization_damping",    0.1_f,
        "Used in stabilization phase. Specifies the damping coefficient of particle velocities." },

    /// Global parameters of N-body simulations
    { RunSettingsId::NBODY_INERTIA_TENSOR,          "nbody.inertia_tensor",     false,
        "If true, each particle has generally non-isotropic inertia tensor. The inertia tensor is evolved using "
        "Euler's equations. Particle geometry is still spherical though; particles always collide as two spheres "
        "and merge into a larger sphere, the inertia tensor is summed up using parallel axis theorem." },
    { RunSettingsId::NBODY_MAX_ROTATION_ANGLE,      "nbody.max_rotation_angle", 0.5_f,
        "Maximum angle of rotation in a single iteration. " },

    /// Gravity
    { RunSettingsId::GRAVITY_SOLVER,                "gravity.solver",           GravityEnum::BARNES_HUT,
        "Solver for computing gravitational acceleration. Can be one of the following:\n" + EnumMap::getDesc<GravityEnum>() },
    { RunSettingsId::GRAVITY_OPENING_ANGLE,         "gravity.opening_angle",    0.5_f,
        "Opening angle (in radians) used in Barnes-Hut algorithm. Larger values means faster gravity evaluation "
        "at a cost of lower precision." },
    { RunSettingsId::GRAVITY_LEAF_SIZE,             "gravity.leaf_size",        25,
        "Maximum number of particles in the leaf of K-d tree." },
    { RunSettingsId::GRAVITY_MULTIPOLE_ORDER,       "gravity.multipole_order",  3,
        "Maximum order of gravitational moments. Use 0 for monopole, 2 for quadrupole, etc. " },
    { RunSettingsId::GRAVITY_KERNEL,                "gravity.kernel",           GravityKernelEnum::SPH_KERNEL,
        "Smoothing kernel of gravity. Can be one of the following:\n" + EnumMap::getDesc<GravityKernelEnum>() },
    { RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD,  "gravity.recomputation_period", 0._f,
        "Period of gravity evaluation. If zero, gravity is computed every time step, for any positive value, "
        "gravitational acceleration is cached for each particle and used each time step until the next "
        "recomputation." },

    /// Collision handling
    { RunSettingsId::COLLISION_HANDLER,             "collision.handler",                CollisionHandlerEnum::MERGE_OR_BOUNCE,
        "Specifies a handler used to evaluate the result of particle collisions. Can be one of the following:\n" + EnumMap::getDesc<CollisionHandlerEnum>() },
    { RunSettingsId::COLLISION_OVERLAP,             "collision.overlap",                OverlapEnum::REPEL,
        "Specifies a handler used to resolve particle overlaps. Can be one of the following:\n" + EnumMap::getDesc<OverlapEnum>() },
    { RunSettingsId::COLLISION_RESTITUTION_NORMAL,  "collision.restitution_normal",     0.8_f,
        "Restitution coefficient of the normal component of velocity. 1 means perfect bounce (no dissipation), "
        "0 means perfect sticking." },
    { RunSettingsId::COLLISION_RESTITUTION_TANGENT, "collision.restitution_tangent",    1.0_f,
        "Restitution coefficient of the tangential component of velocity. Should be 1 to conserve the total "
        "angular momentum. " },
    { RunSettingsId::COLLISION_ALLOWED_OVERLAP,     "collision.allowed_overlap",        0.01_f,
        "Maximum relative overlap of particle that is still classified as collision rather than overlap. Needed "
        "mainly for numerical reasons (floating-point arithmetics). "},
    { RunSettingsId::COLLISION_MERGING_LIMIT,       "collision.merging_limit",          1._f,
        "Multiplier of the relative velocity and the angular velocity of the merger, used when determining "
        "whether to merge the collided particles or reject the collision. If zero, particles are always merged, "
        "values slightly lower than 1 can be used to simulate strength, holding together a body rotating above "
        "the breakup limit. Larger values can be used to merge only very slowly moving particles." },

    /// Timestepping parameters
    { RunSettingsId::TIMESTEPPING_INTEGRATOR,       "timestep.integrator",      TimesteppingEnum::PREDICTOR_CORRECTOR,
        "Integrator performing evolution in time. Can be one of the following:\n" + EnumMap::getDesc<TimesteppingEnum>() },
    { RunSettingsId::TIMESTEPPING_COURANT_NUMBER,   "timestep.courant_number",         1._f,
        "Courant number limiting the time step value. Needed for numerical stability of the integrator. Always keep <= 1!" },
    { RunSettingsId::TIMESTEPPING_MAX_TIMESTEP,     "timestep.max_step",        0.1_f,
        "Maximal allowed value of the time step." },
    { RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, "timestep.initial",         0.03_f,
        "Initial time step of the simulation. "},
    { RunSettingsId::TIMESTEPPING_CRITERION,        "timestep.criterion",       TimeStepCriterionEnum::ALL,
        "Criteria limiting the value of the time step. Can be one or more values from:\n" + EnumMap::getDesc<TimeStepCriterionEnum>() },
    { RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR,  "timestep.adaptive.factor", 0.2_f,
        "Multiplicative factor of the time step computed as a value-to-derivative ratio of time-dependent quantities."},
    { RunSettingsId::TIMESTEPPING_MEAN_POWER,       "timestep.mean_power",      -INFTY,
        "Power of the generalized mean, used to compute the final timestep from timesteps of individual "
        "particles. Negative infinity means the minimal timestep is used. This value will also set statistics "
        "of the restricting particle, namely the particle index and the quantity value and corresponding "
        "derivative of the particle; these statistics are not saved for other powers." },
    { RunSettingsId::TIMESTEPPING_MAX_CHANGE,       "timestep.max_change",      INFTY,
        "Maximum relative difference between time steps in subsequent iterations. Use to 'smooth' the integration and "
        "to avoid rapid changes of time steps."},
    { RunSettingsId::TIMESTEPPING_MIDPOINT_COUNT,   "timestep.midpoint_count",  5,
        "Applicable for modified midpoint method. Specified the number of sub-steps within one time step." },
    { RunSettingsId::TIMESTEPPING_BS_ACCURACY,      "timestep.bs.accuracy",     1.e-3_f,
        "Required relative accuracy (epsilon value) of the Bulirsch-Stoer integrator." },

    /// Selected coordinate system, rotation of bodies
    { RunSettingsId::FRAME_ANGULAR_FREQUENCY,       "frame.angular_frequency",  Vector(0._f),
        "Used to perform the simulation in rotating (non-inertial) frame. Specifies a global rotation of the coordinate "
        "system around axis (0, 0, 1) passing through origin. If the solver includes inertial forces, rotating frame "
        "introduces centrifugal and Coriolis force." },

    /// Computational domain and boundary conditions
    { RunSettingsId::DOMAIN_TYPE,                   "domain.type",              DomainEnum::NONE,
        "Computational domain of the simulation. Applicable only if boundary conditions are specified. "
        "Can be one of the following:\n" + EnumMap::getDesc<DomainEnum>() },
    { RunSettingsId::DOMAIN_BOUNDARY,               "domain.boundary",          BoundaryEnum::NONE,
        "Boundary conditions of the simulation. Can be one of the following:\n" + EnumMap::getDesc<BoundaryEnum>() },
    { RunSettingsId::DOMAIN_GHOST_MIN_DIST,         "domain.ghosts.min_dist",   0.1_f,
        "Used by GhostParticles boundary condition. Specifies the minimal distance between a particle and its ghost, "
        "in units of smoothing length. Used to avoid an overlap of particles."},
    { RunSettingsId::DOMAIN_FROZEN_DIST,            "domain.frozen_dist",       2.5_f, /// \todo this should really be the kernel radius
        "Used by FrozenParticles boundary condition. Specifies the freezing distance from the boundary, "
        "in units of smoothing length." },
    { RunSettingsId::DOMAIN_CENTER,                 "domain.center",            Vector(0._f),
        "Center of the computational domain." },
    { RunSettingsId::DOMAIN_RADIUS,                 "domain.radius",            1._f,
        "Radius of the computational domain. Used by spherical and cylindrical domain." },
    { RunSettingsId::DOMAIN_HEIGHT,                 "domain.height",            1._f,
        "Height of the computational domain. Used by cylindrical domain." },
    { RunSettingsId::DOMAIN_SIZE,                   "domain.size",              Vector(1._f),
        "Dimensions of the computational domain. Used by block and ellipsoidal domain." }
});

template<>
AutoPtr<BodySettings> BodySettings::instance (new BodySettings {
    /// Equation of state
    { BodySettingsId::EOS,                     "eos",                          EosEnum::TILLOTSON,
        "Equation of state for this material. Can be one of the following:\n" + EnumMap::getDesc<EosEnum>() },
    { BodySettingsId::ADIABATIC_INDEX,         "eos.adiabatic_index",          1.4_f,
        "Adiabatic index of gass, applicable for ideal gass EoS." },
    { BodySettingsId::TAIT_GAMMA,              "eos.tait.gamma",               7._f,
        "Density exponent of Tait EoS." },
    { BodySettingsId::TAIT_SOUND_SPEED,        "eos.tait.sound_speed",         1484._f, // value for water
        "Sound speed used by Tait EoS." },
    { BodySettingsId::TILLOTSON_SMALL_A,       "eos.tillotson.small_a",        0.5_f,
        "Tilloson parameter a." },
    { BodySettingsId::TILLOTSON_SMALL_B,       "eos.tillotson.small_b",        1.5_f,
        "Tillotson parameter b." },
    { BodySettingsId::TILLOTSON_ALPHA,         "eos.tillotson.alpha",          5._f,
        "Tillotson parameter alpha." },
    { BodySettingsId::TILLOTSON_BETA,          "eos.tillotson.beta",           5._f,
        "Tillotson parameter beta." },
    { BodySettingsId::TILLOTSON_NONLINEAR_B,   "eos.tillotson.nonlinear_b",    2.67e10_f,
        "Tillotsont parameter B." },
    { BodySettingsId::TILLOTSON_SUBLIMATION,   "eos.tillotson.sublimation",    4.87e8_f,
        "Specific energy of sublimation." },
    { BodySettingsId::TILLOTSON_ENERGY_IV,     "eos.tillotson.energy_iv",      4.72e6_f,
        "Specific energy of incipient vaporization, used in Tillotson EoS." },
    { BodySettingsId::TILLOTSON_ENERGY_CV,     "eos.tillotson.energy_cv",      1.82e7_f,
        "Specific energy of complete vaporization, used in Tillotson EoS." },
    { BodySettingsId::GRUNEISEN_GAMMA,         "eos.mie_gruneisen.gamma",      2._f,      // value for copper taken from wikipedia
        "Gruneisen gamma, used in Mie-Gruneisen EoS." },
    { BodySettingsId::HUGONIOT_SLOPE,          "eos.mie_gruneises.hugoniot_slope",   1.5_f, // value for copper taken from wikipedia
        "Slope of the Hugoniot curve, used in Mie-Gruneisen EoS." },
    { BodySettingsId::BULK_SOUND_SPEED,        "eos.mie_gruneises.bulk_sound_speed", 3933._f,  // value for copper taken from wikipedia
        "Bulk sound speed used in Mie-Gruneisen EoS." },

    /// Yielding & Damage
    { BodySettingsId::RHEOLOGY_YIELDING,    "rheology.yielding",            YieldingEnum::VON_MISES,
        "Specifies the rheology of this material. Can be one of the following:\n" + EnumMap::getDesc<YieldingEnum>() },
    { BodySettingsId::RHEOLOGY_DAMAGE,      "rheology.damage",              FractureEnum::SCALAR_GRADY_KIPP,
        "Fracture model of this material. Can be one of the following:\n" + EnumMap::getDesc<FractureEnum>() },
    { BodySettingsId::ELASTICITY_LIMIT,     "rheology.elasticity_limit",    3.5e9_f,
        "Elasticity limit of the von Mises yielding criterion, specifying the stress of transition between elastic "
        "and plastic deformation." },
    { BodySettingsId::MELT_ENERGY,          "rheology.melt_energy",         3.4e6_f,
        "Specific melting energy, used by von Mises criterion." },
    { BodySettingsId::COHESION,             "rheology.cohesion",            9.e7_f,
        "Cohesion, yield strength at zero pressure. Used by Drucker-Prager rheology." },
    { BodySettingsId::INTERNAL_FRICTION,    "rheology.internal_friction",   2._f,
        "Coefficient of friction for undamaged material. Used by Drucker-Prager rheology." },
    { BodySettingsId::DRY_FRICTION,         "rheology.dry_friction",        0.8_f,
        "Coefficient of friction for fully damaged material. Used by Drucker-Prager rheology." },
    { BodySettingsId::BRITTLE_DUCTILE_TRANSITION_PRESSURE, "rheology.brittle_ductile_transition_pressure", 1.23e9_f },  // Modeling damage and deformation in impact simulations
    { BodySettingsId::BRITTLE_PLASTIC_TRANSITION_PRESSURE, "rheology.brittle_plastic_transition_pressure", 2.35e9_f },  // Collions et al. (2004)
    { BodySettingsId::MOHR_COULOMB_STRESS,  "rheology.mohr_coulomb_stress", 0._f },
    { BodySettingsId::FRICTION_ANGLE,       "rheology.friction_angle",      0._f },

    /// Material properties
    { BodySettingsId::DENSITY,                 "material.density",             2700._f,
        "Initial density of the material." },
    { BodySettingsId::DENSITY_RANGE,           "material.density.range",       Interval(10._f, INFTY),
        "Allowed range of densities for this material." },
    { BodySettingsId::DENSITY_MIN,             "material.density.min",         50._f,
        "Scale value for density, used to determine the time step value from derivatives of density." },
    { BodySettingsId::ENERGY,                  "material.energy",              0._f,
        "Initial specific energy of the material." },
    { BodySettingsId::ENERGY_RANGE,            "material.energy.range",        Interval(0._f, INFTY),
        "Allowed range of specific energy." },
    { BodySettingsId::ENERGY_MIN,              "material.energy.min",          1._f,
        "Scale value for specific energy, used to determine the time step value from derivatives of energy." },
    { BodySettingsId::DAMAGE,                  "material.damage",              0._f,
        "Initial damage of the material." },
    { BodySettingsId::DAMAGE_RANGE,            "material.damage.range",        Interval(0.f, 1._f),
        "Allowed range of damage." },
    { BodySettingsId::DAMAGE_MIN,              "material.damage.min",          0.03_f,
        "Scale value for damage, used to determine the time step value from derivatives of damage." },
    { BodySettingsId::STRESS_TENSOR,           "material.stress_tensor",       TracelessTensor(0._f),
        "Initial value of the deviatoric stress tensor (components xx, yy, xy, xz, yz)." },
    { BodySettingsId::STRESS_TENSOR_MIN,       "material.stress_tensor.min",   1.e5_f,
        "Scale value for deviatoric stress, used to determine the time step value from derivatives of stress." },
    { BodySettingsId::BULK_MODULUS,            "material.bulk_modulus",        2.67e10_f,
        "Bulk modulus of the material." },
    { BodySettingsId::SHEAR_MODULUS,           "material.shear_modulus",       2.27e10_f,
        "Shear modulus of the material."},
    { BodySettingsId::YOUNG_MODULUS,           "material.young_modulus",       5.7e10_f,
        "Young modulus of the material."},
    { BodySettingsId::ELASTIC_MODULUS,         "material.elastic_modulus",     8.e9_f, /// \todo use the value of Basalt, this one is for quartz from Wiki
        "Elastic modulus of the material." },
    { BodySettingsId::RAYLEIGH_SOUND_SPEED,    "material.rayleigh_speed",      0.4_f,
        "Speed of crack propagation, in units of local sound speed." },
    { BodySettingsId::WEIBULL_COEFFICIENT,     "material.weibull_coefficient", 4.e35_f,
        "Coefficient k of Weibull distribution." },
    { BodySettingsId::WEIBULL_EXPONENT,        "material.weibull_exponent",    9._f,
        "Coefficienet m of Weibull distribution." },
    { BodySettingsId::BULK_VISCOSITY,          "material.bulk_viscosity",      1.e20_f,
        "Bulk viscosity of the material. Applicable is internal friction is used." },
    { BodySettingsId::SHEAR_VISCOSITY,         "material.shear_viscosity",     1.e20_f,
        "Shear viscosity of the material. Applicable is internal friction is used." },
    { BodySettingsId::DIFFUSIVITY,             "material.diffusivity",         1._f,
        "Diffusivity of the material, used in heat diffusion equation." },
    { BodySettingsId::SURFACE_TENSION,         "material.surface_tension",     1._f,
        "Surface tension of the fluid. Not applicable for solids nor gass." },

    /// SPH parameters specific for the body
    { BodySettingsId::INITIAL_DISTRIBUTION,    "sph.initial_distribution",     DistributionEnum::HEXAGONAL,
        "Initial distribution of the particles in space. Can be one of the following:\n" + EnumMap::getDesc<DistributionEnum>() },
    { BodySettingsId::CENTER_PARTICLES,        "sph.center_particles",         true,
        "If true, generated particles will be moved so that their center of mass corresponds to the center of "
        "selected domain. Note that this will potentially move some particles outside of the domain, which can "
        "clash with some boundary conditions." },
    { BodySettingsId::PARTICLE_SORTING,        "sph.particle_sorting",         false,
        "If true, particles are shuffle in storage according to their Morton code, so that locality in space "
        "implies locality in memory. Reading and writing quantities can be faster because of that." },
    { BodySettingsId::DISTRIBUTE_MODE_SPH5,    "sph.distribute_mode_sph5",     false,
        "Turns on 'SPH5 compatibility' mode when generating particle positions. This allows 1-1 comparison of "
        "generated arrays, but results in too many generated particles (by about factor 1.4). The option also "
        "implies center_particles = true." },
    { BodySettingsId::DIELH_STRENGTH,          "sph.diehl_strength",           0.1_f,
        "Magnitude of the particle displacement in a single iteration. Used by Diehl's distribution." },
    { BodySettingsId::DIEHL_MAX_DIFFERENCE,    "sph.diehl_max_difference",     10,
        "Maximum allowed difference between the expected number of particles and the actual number of generated "
        "particles. Higher value speed up the generation of particle positions." },
    { BodySettingsId::PARTICLE_COUNT,          "sph.particle_count",           10000,
        "Required number of particles in the body. Note that the actual number of particles may differ, depending "
        "on the selected distribution. " },
    { BodySettingsId::MIN_PARTICLE_COUNT,      "sph.min_particle_count",       100,
         "Minimal number of particles per one body. Used when creating 'sub-bodies' withing one 'parent' body, "
         "for example when creating rubble-pile asteroids, ice blocks inside an asteroid, etc. Parameter has no "
         "effect for creation of a single monolithic body; the number of particles from PARTICLE_COUNT is used "
         "in any case." },
    { BodySettingsId::AV_ALPHA,                "av.alpha",                     1.5_f,
        "Initial coefficient alpha of the Morris-Monaghan artificial viscosity. Beta coefficient of the viscosity "
        "is derived as 2*alpha." },
    { BodySettingsId::AV_ALPHA_RANGE,          "av.alpha.range",               Interval(0.05_f, 1.5_f),
        "Allowed range of the alpha coefficient. Used by Morris-Monaghan artificial viscosity."},
    { BodySettingsId::BODY_CENTER,             "body.center",                  Vector(0._f),
        "Center of the body. Needed by stabilization solver to correctly compute velocities in co-moving "
        "(co-rotating) frame." },
    { BodySettingsId::BODY_VELOCITY,           "body.velocity",                Vector(0._f),
        "Initial velocity of the body. Needed by stabilization solver to correctly compute velocities in "
        "co-moving (co-rotating) frame." },
    { BodySettingsId::BODY_ANGULAR_VELOCITY,   "body.angular_velocity",        Vector(0._f),
      "Initial angular frequency of the body. Needed by stabilization solver to correctly compute velocities "
      "in co-moving (co-rotating) frame." },
});

// clang-format on


// Explicit instantiation
template class Settings<BodySettingsId>;
template class SettingsIterator<BodySettingsId>;

template class Settings<RunSettingsId>;
template class SettingsIterator<RunSettingsId>;

NAMESPACE_SPH_END
