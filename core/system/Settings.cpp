#include "gravity/AggregateSolver.h"
#include "io/Output.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

static auto DEFAULT_QUANTITY_IDS = OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY |
                                   OutputQuantityFlag::SMOOTHING_LENGTH | OutputQuantityFlag::MASS |
                                   OutputQuantityFlag::DENSITY | OutputQuantityFlag::PRESSURE |
                                   OutputQuantityFlag::ENERGY;


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

static RegisterEnum<KernelEnum> sKernel({
    { KernelEnum::CUBIC_SPLINE, "cubic_spline", "M4 B-spline (piecewise cubic polynomial" },
    { KernelEnum::FOURTH_ORDER_SPLINE, "fourth_order_spline", "M5 B-spline (piecewise 4th-order polynomial" },
    { KernelEnum::GAUSSIAN, "gaussian", "Gaussian function with clamped support" },
    { KernelEnum::TRIANGLE,
        "triangle",
        "Triangular (piecewise linear) kernel. Derivatives are not continuous, the kernel is therefore not "
        "suitable for SPH, but can be useful for non-SPH interpolations, etc." },
    { KernelEnum::CORE_TRIANGLE, "core_triangle", "Core Triangle (CT) kernel by Read et al. (2010)" },
    { KernelEnum::THOMAS_COUCHMAN,
        "thomas_couchman",
        "Modification of the M4 B-spline kernel by Thomas & Couchman (1992), designed to prevent clustering "
        "of particles." },
    { KernelEnum::WENDLAND_C2, "wendland_c2", "Wendland kernel C2" },
    { KernelEnum::WENDLAND_C4, "wendland_c4", "Wendland kernel C4" },
    { KernelEnum::WENDLAND_C6, "wendland_c6", "Wendland kernel C6" },
});

static RegisterEnum<TimesteppingEnum> sTimestepping({
    { TimesteppingEnum::EULER_EXPLICIT, "euler_explicit", "Explicit (forward) 1st-order integration" },
    { TimesteppingEnum::LEAP_FROG, "leap_frog", "Leap-frog 2nd-order integration" },
    { TimesteppingEnum::RUNGE_KUTTA, "runge_kutta", "Runge-Kutta 4-th order integration" },
    { TimesteppingEnum::PREDICTOR_CORRECTOR, "predictor_corrector", "Predictor-corrector scheme" },
    { TimesteppingEnum::MODIFIED_MIDPOINT,
        "modified_midpoint",
        "Modified midpoint method with constant number of substeps." },
    { TimesteppingEnum::BULIRSCH_STOER, "bulirsch_stoer", "Bulirsch-Stoer integrator" },
});

static RegisterEnum<TimeStepCriterionEnum> sTimeStepCriterion({
    { TimeStepCriterionEnum::COURANT, "courant", "Time step determined using CFL condition" },
    { TimeStepCriterionEnum::DERIVATIVES,
        "derivatives",
        "Time step computed by limiting value-to-derivative ratio of quantities" },
    { TimeStepCriterionEnum::DIVERGENCE, "divergence", "Time step computed from velocity divergence." },
    { TimeStepCriterionEnum::ACCELERATION,
        "acceleration",
        "Time step computed from ratio of acceleration and smoothing length." },
});

static RegisterEnum<FinderEnum> sFinder({
    { FinderEnum::BRUTE_FORCE,
        "brute_force",
        "Brute-force search by going through each pair of particles (O(N^2) complexity)" },
    { FinderEnum::KD_TREE, "kd_tree", "Using K-d tree" },
    { FinderEnum::OCTREE, "octree", "Using octree" },
    { FinderEnum::LINKED_LIST, "linked_list", "Using linked list" },
    { FinderEnum::UNIFORM_GRID, "uniform_grid", "Partitioning particles into a grid uniform in space" },
    { FinderEnum::HASH_MAP, "hash_map", "Using hash map" },
});

static RegisterEnum<BoundaryEnum> sBoundary({
    { BoundaryEnum::NONE, "none", "Do not use any boundary conditions (= vacuum conditions)" },
    { BoundaryEnum::FROZEN_PARTICLES,
        "frozen_particles",
        "Highest derivatives of all particles close to the boundary are set to zero." },
    { BoundaryEnum::GHOST_PARTICLES,
        "ghost_particles",
        "Create ghosts particles located symmetricaly to the particles near the boundary, in order to keep "
        "particles inside domain." },
    { BoundaryEnum::FIXED_PARTICLES,
        "fixed_particles",
        "Boundary is created by immovable particles placed along the boundary." },
    { BoundaryEnum::WIND_TUNNEL,
        "wind_tunnel",
        "Simulates a wind tunnel by pushing air particles into the domain and removing them on the other "
        "side of the domain. The air particles are kept inside the domain using Frozen Particles boundary "
        "conditions." },
    { BoundaryEnum::PERIODIC,
        "periodic",
        "Periodic boundary conditions; particles can interact accross boundaries. When particles leave the "
        "domain, they re-enter on the other side of the domain. " },
    { BoundaryEnum::SYMMETRIC, "symmetric", "Particles are duplicated along the z=0 plane." },
    { BoundaryEnum::KILL_ESCAPERS, "kill_escapers", "Removes all particles outside the domain" },
    { BoundaryEnum::PROJECT_1D,
        "project_1D",
        "Debug boundary condition, used to emulate 1D SPH solver. While the solver is still "
        "three-dimensional under the hood, the particles are projected on a line and can move only in one "
        "dimension. Note that this has to be supplied by correct kernel normalization, etc." },
});

static RegisterEnum<DomainEnum> sDomain({ { DomainEnum::NONE, "none", "No computational domain." },
    { DomainEnum::SPHERICAL, "spherical", "Sphere with given radius." },
    { DomainEnum::ELLIPSOIDAL, "ellipsoidal", "Axis-aligned ellipsoidal domain." },
    { DomainEnum::BLOCK, "block", "Axis-aligned block domain." },
    { DomainEnum::CYLINDER, "cylinder", "Cylindrical domain aligned with z axis." } });

static RegisterEnum<ForceEnum> sForce({
    { ForceEnum::PRESSURE, "pressure", "Force given by pressure gradient." },
    { ForceEnum::SOLID_STRESS,
        "solid_stress",
        "Use force from stress divergence in the model. Must be used together with pressure gradient. Stress "
        "tensor is evolved in time using Hooke's equation." },
    { ForceEnum::NAVIER_STOKES,
        "navier_stokes",
        "Stress tensor for the simulation of fluids. Must be used together with pressure gradient, cannot be "
        "used together with solid stress force." },
    { ForceEnum::INERTIAL,
        "inertial",
        "Centrifugal force and Coriolis force given by angular frequency of the coordinate frame." },
    { ForceEnum::SELF_GRAVITY, "gravity", "Self-gravity of particles" },
    { ForceEnum::SURFACE_TENSION,
        "surface_tension",
        "Surface tension, proportional to the curvature of the surface." },
});

static RegisterEnum<ArtificialViscosityEnum> sArtificialViscosity({
    { ArtificialViscosityEnum::NONE, "none", "No artificial viscosity" },
    { ArtificialViscosityEnum::STANDARD,
        "standard",
        "Standard artificial viscosity term by Monaghan (1989)." },
    { ArtificialViscosityEnum::RIEMANN,
        "riemann",
        "Artificial viscosity term analogous to Riemann solvers by Monaghan (1997)." },
    { ArtificialViscosityEnum::MORRIS_MONAGHAN,
        "morris_monaghan",
        "Time-dependent artificial viscosity by Morris & Monaghan (1997)." },
});

static RegisterEnum<SolverEnum> sSolver({
    { SolverEnum::SYMMETRIC_SOLVER,
        "symmetric_solver",
        "SPH solver using symmetrized evaluation of derivatives. Cannot be used together with some "
        "parameters, for example with strain rate correction tensor!" },
    { SolverEnum::ASYMMETRIC_SOLVER,
        "asymmetric_solver",
        "SPH solver evaluating all derivatives asymmetrically." },
    { SolverEnum::SUMMATION_SOLVER,
        "summation_solver",
        "Solver computing density by direct summation over nearest SPH particles." },
    { SolverEnum::ELASTIC_DEFORMATION_SOLVER,
        "elastic_deformation_solver",
        "Solver used for extreme elastic deformations." },
    { SolverEnum::DENSITY_INDEPENDENT,
        "density_independent",
        "Density independent solver by Saitoh & Makino (2013). Experimental!" },
    { SolverEnum::ENERGY_CONSERVING_SOLVER,
        "energy_conserving_solver",
        "Solver advancing internal energy using pair-wise work done by particles, by Owen (2009). "
        "Experimental!" },
    { SolverEnum::SIMPLE_SOLVER,
        "simple_solver",
        "Simple solver with pressure gradient only, mainly used for supporting purposes (benchmarking, "
        "teaching, etc.)" },
});

static RegisterEnum<ContinuityEnum> sContinuity({
    { ContinuityEnum::STANDARD,
        "standard",
        "Normal continuity equation, using velocity divergence "
        "computed from all neighbors." },
    { ContinuityEnum::SUM_ONLY_UNDAMAGED,
        "sum_only_undamaged",
        "Computes the velocity divergence using only undamaged neighbors. For fully damaged particle, the "
        "standard continuity equation is used instead." },
});

static RegisterEnum<DiscretizationEnum> sFormulation({
    { DiscretizationEnum::STANDARD,
        "standard",
        "Standard discretization of SPH equations. Equations are obtained from Lagrangian." },
    { DiscretizationEnum::BENZ_ASPHAUG,
        "benz_asphaug",
        "Alternative formulation of SPH, used by Benz & Asphaug (1994, 1995)." },
});

static RegisterEnum<YieldingEnum> sYield({
    { YieldingEnum::NONE, "none", "No stress tensor, gass or material with no stress tensor" },
    { YieldingEnum::ELASTIC, "elastic", "No yield, just elastic deformations following Hooke's law" },
    { YieldingEnum::VON_MISES, "von_mises", "Stress yielding using von Mises criterion." },
    { YieldingEnum::DRUCKER_PRAGER, "drucker_prager", "Drucker-Prager pressure dependent yielding stress." },
    { YieldingEnum::DUST, "dust", "No stress tensor, only the pressure is limited to positive values." },
});

static RegisterEnum<FractureEnum> sFracture({
    { FractureEnum::NONE, "none", "No fragmentation" },
    { FractureEnum::SCALAR_GRADY_KIPP,
        "scalar_grady_kipp",
        "Grady-Kipp model of fragmentation using scalar damage" },
    { FractureEnum::TENSOR_GRADY_KIPP,
        "tensor_grady_kipp",
        "Grady-Kipp model of fragmentation using tensor damage" },
});

static RegisterEnum<SmoothingLengthEnum> sSmoothingLength({
    { SmoothingLengthEnum::CONTINUITY_EQUATION,
        "continuity_equation",
        "Smoothing length is evolved using continuity equation." },
    { SmoothingLengthEnum::SOUND_SPEED_ENFORCING,
        "sound_speed_enforcing",
        "Number of neighbors is kept in the specified range by adding additional derivatives of smoothing "
        "length, scaled by local sound speed." },
});

static RegisterEnum<SignalSpeedEnum> sSignalSpeed({
    { SignalSpeedEnum::PRESSURE_DIFFERENCE,
        "pressure_difference",
        "Signal speed given by the absolute value of pressure difference, as in Price (2008). Cannot be used "
        "in simulations with gravity." },

    { SignalSpeedEnum::VELOCITY_DIFFERENCE,
        "velocity_difference",
        "Signal speed given by relative velocity projected to the positive vector, as in Valdarnini "
        "(2018)." },
});

static RegisterEnum<GravityEnum> sGravity({
    { GravityEnum::SPHERICAL,
        "spherical",
        "No self-gravity, particles only move in spherically symmetric gravitational potential. Can be used "
        "as an approximate gravity for spherically symmetric simulations." },
    { GravityEnum::BRUTE_FORCE,
        "brute_force",
        "Brute-force summation over all particle pairs (O(N^2) complexity)" },
    { GravityEnum::BARNES_HUT,
        "barnes_hut",
        "Barnes-Hut algorithm approximating gravity by multipole expansion (up to octupole order)." },
});

static RegisterEnum<GravityKernelEnum> sGravityKernel({
    { GravityKernelEnum::POINT_PARTICLES, "point_particles", "Point-like particles with zero radius." },
    { GravityKernelEnum::SPH_KERNEL,
        "sph_kernel",
        "Smoothing kernel associated with selected SPH kernel. For SPH simulations." },
    { GravityKernelEnum::SOLID_SPHERES,
        "solid_spheres",
        "Kernel representing gravity of solid spheres. Useful for N-body simulations where overlaps are "
        "allowed." },
});

static RegisterEnum<CollisionHandlerEnum> sCollisionHandler({
    { CollisionHandlerEnum::NONE, "none", "No handling of collisions." },
    { CollisionHandlerEnum::PERFECT_MERGING,
        "perfect_merging",
        "All collided particles merge, creating larger spherical particles. Particles are merged "
        "unconditionally, regardless of their relative velocity or their angular frequencies." },
    { CollisionHandlerEnum::ELASTIC_BOUNCE,
        "elastic_bounce",
        "Collided particles bounce with some energy dissipation, specified by the coefficients of "
        "restitution. No merging, number of particles remains constant." },
    { CollisionHandlerEnum::MERGE_OR_BOUNCE,
        "merge_or_bounce",
        "If the relative speed of the collided particles is lower than the escape velocity, the particles "
        "are merged, otherwise the particle bounce. To ensure that the particles are always merged, set the "
        "collision.merging_limit to zero, on the other hand large values make particles more difficult to "
        "merge." },
});

static RegisterEnum<OverlapEnum> sOverlap({
    { OverlapEnum::NONE, "none", "All overlaps are ignored." },
    { OverlapEnum::FORCE_MERGE, "force_merge", "Overlapping particles are merged." },
    { OverlapEnum::REPEL, "repel", "Particles are shifted until no overlap happens." },
    { OverlapEnum::REPEL_OR_MERGE,
        "repel_or_merge",
        "Particles are either repeled (and bounced) or merged, based on the ratio of their relative velocity "
        "to the escape velocity (similar to merge_or_bounce collision handler)." },
    { OverlapEnum::INTERNAL_BOUNCE,
        "internal_bounce",
        "If the center of the particles are moving towards each other, particles bounce, otherwise nothing "
        "happens." },
    { OverlapEnum::PASS_OR_MERGE,
        "pass_or_merge",
        "Overlap is allowed. If the relative velocity of particles is lower than the escape velocity, "
        "particles are merged, otherwise they simply pass through each other." },
});

static RegisterEnum<LoggerEnum> sLogger({
    { LoggerEnum::NONE, "none", "Do not log anything." },
    { LoggerEnum::STD_OUT, "stdout", "Print log to standard output." },
    { LoggerEnum::FILE, "file", "Print log to a file." },
});

static RegisterEnum<IoEnum> sIo({
    { IoEnum::NONE, "none", "No output" },
    { IoEnum::DATA_FILE,
        "data_file",
        "Binary output file, containing only few selected quantities. This is the most convenient "
        "format for storing full simulation in high resolution in time. Cannot be used to continue "
        "simulation." },
    { IoEnum::BINARY_FILE,
        "binary_file",
        "Save output data into binary file. This data dump is lossless and can be use to restart run from "
        "saved snapshot. Stores values, all derivatives and materials of the storage." },
    { IoEnum::TEXT_FILE, "text_file", "Save output data into formatted human-readable text file" },
    { IoEnum::VTK_FILE,
        "vtk_file",
        "File format used by Visualization Toolkit (VTK). Useful to view the results in Paraview and other "
        "visualization tools." },
    { IoEnum::HDF5_FILE,
        "hdf5_file",
        "File format for storing scientific data. Currently tailored for files generated by the code "
        "miluphcuda. Requires to build the code with libhdf5." },
    { IoEnum::MPCORP_FILE, "mpcorp_file", "Export from Minor Planet Center Orbit Database" },
    { IoEnum::PKDGRAV_INPUT, "pkdgrav_input", "Generate a pkdgrav input file." },
});

Optional<String> getIoExtension(const IoEnum type) {
    switch (type) {
    case IoEnum::NONE:
        return NOTHING;
    case IoEnum::TEXT_FILE:
        return String("txt");
    case IoEnum::BINARY_FILE:
        return String("ssf");
    case IoEnum::DATA_FILE:
        return String("sdf");
    case IoEnum::PKDGRAV_INPUT:
        return String("ss");
    case IoEnum::VTK_FILE:
        return String("vtu");
    case IoEnum::HDF5_FILE:
        return String("h5");
    case IoEnum::MPCORP_FILE:
        return String("dat");
    default:
        NOT_IMPLEMENTED;
    }
}

Optional<IoEnum> getIoEnum(const String& ext) {
    if (ext == "txt") {
        return IoEnum::TEXT_FILE;
    } else if (ext == "ssf") {
        return IoEnum::BINARY_FILE;
    } else if (ext == "sdf" || ext == "scf") { // legacy handling
        return IoEnum::DATA_FILE;
    } else if (ext == "ss") {
        return IoEnum::PKDGRAV_INPUT;
    } else if (ext == "vtu") {
        return IoEnum::VTK_FILE;
    } else if (ext == "h5") {
        return IoEnum::HDF5_FILE;
    } else if (ext == "dat") {
        return IoEnum::MPCORP_FILE;
    } else {
        return NOTHING;
    }
}

String getIoDescription(const IoEnum type) {
    switch (type) {
    case IoEnum::NONE:
        NOT_IMPLEMENTED;
    case IoEnum::TEXT_FILE:
        return "Plain text file";
    case IoEnum::BINARY_FILE:
        return "SPH state file";
    case IoEnum::DATA_FILE:
        return "SPH data file";
    case IoEnum::PKDGRAV_INPUT:
        return "Pkdgrav output files";
    case IoEnum::VTK_FILE:
        return "VTK unstructured grid";
    case IoEnum::HDF5_FILE:
        return "miluphcuda output file";
    case IoEnum::MPCORP_FILE:
        return "mpcorp dump";
    default:
        NOT_IMPLEMENTED;
    }
}

Flags<IoCapability> getIoCapabilities(const IoEnum type) {
    switch (type) {
    case IoEnum::NONE:
        return EMPTY_FLAGS;
    case IoEnum::TEXT_FILE:
        return IoCapability::INPUT | IoCapability::OUTPUT;
    case IoEnum::BINARY_FILE:
        return IoCapability::INPUT | IoCapability::OUTPUT;
    case IoEnum::DATA_FILE:
        return IoCapability::INPUT | IoCapability::OUTPUT;
    case IoEnum::PKDGRAV_INPUT:
        return IoCapability::INPUT | IoCapability::OUTPUT;
    case IoEnum::VTK_FILE:
        return IoCapability::OUTPUT;
    case IoEnum::HDF5_FILE:
        return IoCapability::INPUT;
    case IoEnum::MPCORP_FILE:
        return IoCapability::INPUT;
    default:
        NOT_IMPLEMENTED;
    }
}

static RegisterEnum<OutputSpacing> sSpacing({
    { OutputSpacing::LINEAR, "linear", "Constant time between consecutive output times" },
    { OutputSpacing::LOGARITHMIC, "logarithmic", "Constant ratio between consecutive output times" },
    { OutputSpacing::CUSTOM, "custom", "User-defined list of output times " },
});

static RegisterEnum<RngEnum> sRng({
    { RngEnum::UNIFORM, "uniform", "Mersenne Twister PRNG from Standard library." },
    { RngEnum::HALTON, "halton", "Halton sequence for quasi-random numbers." },
    { RngEnum::BENZ_ASPHAUG, "benz_asphaug", "RNG used in code SPH5, used for 1-1 comparison of codes." },
});

static RegisterEnum<UvMapEnum> sUv({
    { UvMapEnum::PLANAR, "planar", "Plannar mapping." },
    { UvMapEnum::SPHERICAL, "spherical", "Spherical mapping." },
});


// clang-format off
template<>
const RunSettings& getDefaultSettings() {
    static RunSettings instance({
    { RunSettingsId::RUN_NAME,                      "run.name",                 "unnamed run"_s,
        "User-specified name of the run. Can be stored in the metadata of output files." },
    { RunSettingsId::RUN_COMMENT,                   "run.comment",              ""_s,
        "Auxiliary comment of the run. Can be stored in the metadata of output files." },
    { RunSettingsId::RUN_AUTHOR,                    "run.author",               "Pavel Sevecek"_s,
        "Name of the person performing this run." },
    { RunSettingsId::RUN_EMAIL,                     "run.email",                "sevecek@sirrah.troja.mff.cuni.cz"_s,
        "E-mail of the run author." },
    { RunSettingsId::RUN_TYPE,                      "run.type",                 RunTypeEnum::SPH,
        "Specifies the type of the simulation. Does not have to be specified to run the simulation; this "
        "information is saved in output files and taken into account by visualization tools, for example." },
    { RunSettingsId::RUN_OUTPUT_TYPE,               "run.output.type",          IoEnum::NONE,
        "Format of the output files generated by the run. Can be one of the following:\n" + EnumMap::getDesc<IoEnum>() },
    { RunSettingsId::RUN_OUTPUT_INTERVAL,           "run.output.interval",      0.1_f,
        "Interval of creating output files (in simulation time)." },
    { RunSettingsId::RUN_OUTPUT_SPACING,            "run.output.spacing",       OutputSpacing::LINEAR,
        "Type of output spacing in time. Can be one of the following:\n" + EnumMap::getDesc<OutputSpacing>() },
    { RunSettingsId::RUN_OUTPUT_CUSTOM_TIMES,       "run.output.custom_times",  "0, 0.5, 1, 2, 10"_s,
        "List of comma-separated output times, used when the output spacing is set to 'custom'" },
    { RunSettingsId::RUN_OUTPUT_FIRST_INDEX,        "run.output.first_index",   0,
        "Index of the first generated output file. Might not be zero if the simulation is resumed." },
    { RunSettingsId::RUN_OUTPUT_NAME,               "run.output.name",          "out_%d.txt"_s,
        "File mask of the created files. It can contain a wildcard %d, which is replaced by the output number and "
        "%t which is replaced by current simulation time." },
    { RunSettingsId::RUN_OUTPUT_PATH,               "run.output.path",          "out"_s,
        "Directory where the output files are saved. Can be either absolute or relative path." },
    { RunSettingsId::RUN_OUTPUT_QUANTITIES, "run.output.quantitites", DEFAULT_QUANTITY_IDS,
        "List of quantities to write to output file. Applicable for text and VTK outputs, binary output always stores "
        "all quantitites. Can be one or more values from:\n" + EnumMap::getDesc<OutputQuantityFlag>() },
    { RunSettingsId::RUN_THREAD_CNT,                "run.thread.cnt",           0,
        "Number of threads used by the simulation. 0 means all available threads are used." },
    { RunSettingsId::RUN_THREAD_GRANULARITY,        "run.thread.granularity",   1000,
        "Number of particles processed by one thread in a single batch. Lower number can help to distribute tasks "
        "between threads more evenly, higher number means faster processing of particles within single thread." },
    { RunSettingsId::RUN_LOGGER,                    "run.logger",               LoggerEnum::STD_OUT,
        "Type of a log generated by the simulation. Can be one of the following:\n" + EnumMap::getDesc<LoggerEnum>() },
    { RunSettingsId::RUN_LOGGER_FILE,               "run.logger.file",          "log.txt"_s,
        "Specifies the path where the log is saved (if applicable)" },
    { RunSettingsId::RUN_LOGGER_VERBOSITY,          "run.logger.verbosity",     2,
        "Number specifying log verbosity. Can be between 0 and 3, going from least to most verbose." },
    { RunSettingsId::RUN_VERBOSE_ENABLE,            "run.verbose.enable",       false,
        "Enables verbose log of a simulation. The log is written into a file, specified by parameter run.verbose.name." },
    { RunSettingsId::RUN_VERBOSE_NAME,              "run.verbose.name",         "run.log"_s,
        "Name of a file where the verbose log of the simulation is written." },
    { RunSettingsId::RUN_START_TIME,                "run.start_time",           0._f,
      "Starting time of the simulation in seconds. This is usually 0, although it can be set to a non-zero "
      "for simulations resumed from saved state." },
    { RunSettingsId::RUN_END_TIME,                  "run.end_time",             10._f,
        "End time of the simulation in seconds. For new simulations (not resumed from saved state), this "
        "corresponds to the total duration of the simulation." },
    { RunSettingsId::RUN_TIMESTEP_CNT,              "run.timestep_cnt",         0,
        "Number of timesteps performed by the integrator. If zero, the criterion is not used. " },
    { RunSettingsId::RUN_WALLCLOCK_TIME,            "run.wallclock_time",       0._f,
        "Maximum wallclock time of the simulation. If zero, the criterion is not used. " },
    { RunSettingsId::RUN_RNG,                       "run.rng",                  RngEnum::BENZ_ASPHAUG,
        "Random number generator used by the simulation. Can be one of the following:\n" + EnumMap::getDesc<RngEnum>() },
    { RunSettingsId::RUN_RNG_SEED,                  "run.rng.seed",             1234,
        "Seed of the random number generator (if applicable)." },
    { RunSettingsId::RUN_DIAGNOSTICS_INTERVAL,      "run.diagnostics_interval", 0.1_f,
        "Time period (in run time) of running diagnostics of the run. 0 means the diagnostics are run every time step." },

    /// SPH solvers
    { RunSettingsId::SPH_SOLVER_TYPE,               "sph.solver.type",                  SolverEnum::SYMMETRIC_SOLVER,
        "Selected solver for computing derivatives of physical quantities. Can be one of the following:\n" + EnumMap::getDesc<SolverEnum>() },
    { RunSettingsId::SPH_SOLVER_FORCES,             "sph.solver.forces",                ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS,
        "Forces included in the physical model of the simulation. Can be one or more values from: \n" + EnumMap::getDesc<ForceEnum>() },
    { RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, "sph.adaptive_smoothing_length",    SmoothingLengthEnum::CONTINUITY_EQUATION,
        "Specifies how smoothing length is evolved in the simulation. Can be one or more values from: \n" + EnumMap::getDesc<SmoothingLengthEnum>() },
    { RunSettingsId::SPH_SUMMATION_DENSITY_DELTA,   "sph.summation.density_delta",      1.e-3_f,
        "Used by summation solver. Specifies the relative difference between densities in subsequenct iterations "
        "for which the iterative algorithm is terminated. Lower value means more precise evaluation of density "
        "at a cost of higher computation time. " },
    { RunSettingsId::SPH_SUMMATION_MAX_ITERATIONS,  "sph.summation.max_iterations",  5,
        "Used by summation solver. Specifies the maximum number of iterations for density computation." },
    { RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP,   "sph.asymmetric.compute_radii_hash_map",    false,
        "If true, the SPH solver computes a hash map connecting position in space with required search radius. "
        "Otherwise, the radius is determined from the maximal smoothing length in the simulation. Used only by "
        "the AsymmetricSolver." },
    { RunSettingsId::SPH_USE_XSPH,                  "sph.xsph.enable",          false,
        "Enables the XSPH modification" },
    { RunSettingsId::SPH_XSPH_EPSILON,              "sph.xsph.epsilon",         1._f,
        "Epsilon parameter of XSPH modification." },
    { RunSettingsId::SPH_USE_DELTASPH,              "sph.deltasph.enable",          false,
        "Enables the delta-SPH modification" },
    { RunSettingsId::SPH_DENSITY_DIFFUSION_DELTA,   "sph.deltasph.delta",       0.01_f,
        "Delta-coefficient of the delta-SPH modification, see Marrone et al. 2011." },
    { RunSettingsId::SPH_VELOCITY_DIFFUSION_ALPHA,  "sph.deltasph.alpha",       0.01_f,
        "Alpha-coefficient of the delta-SPH modification." },
    { RunSettingsId::SPH_DI_ALPHA,                  "sph.di.alpha",             1._f,
        "Alpha parameter of the density-independent SPH solver." },
    { RunSettingsId::SPH_SCRIPT_ENABLE,             "sph.script.enable",        false,
        "Whether to enable or disable script evaluation." },
    { RunSettingsId::SPH_SCRIPT_FILE,               "sph.script.file",          "script.chai"_s,
        "Path to the file containing an arbitrary ChaiScript script evaluated each time step." },
    { RunSettingsId::SPH_SCRIPT_PERIOD,             "sph.script.period",        0._f,
        "Period or time point to execute the script. Zero means the time step is executed "
        "immediately or every time step, depending on the value of sph.script.oneshot" },
    { RunSettingsId::SPH_SCRIPT_ONESHOT,            "sph.script.oneshot",       false,
        "Whether to execute the script only once or periodically." },

    /// Global SPH parameters
    { RunSettingsId::SPH_KERNEL,                    "sph.kernel",               KernelEnum::CUBIC_SPLINE,
        "Type of the SPH kernel. Can be one of the following:\n" + EnumMap::getDesc<KernelEnum>() },
    { RunSettingsId::SPH_NEIGHBOR_RANGE,           "sph.neighbor.range",      Interval(25._f, 100._f),
        "Allowed numbers of particle neighbors. Applicable if neighbor enforcing is used for evolution of "
        "smoothing length. Note that even with this parameter set, it is not guaranteed that the number of "
        "neighbors will be within the interval for every particle, the code only tries to do so." },
    { RunSettingsId::SPH_NEIGHBOR_ENFORCING,       "sph.neighbor.enforcing",  0.2_f,
        "'Strength' of the neighbor enforcing. The higher number means the derivative of the smoothing "
        "length can be higher, lower values means 'smoother' evolution of smooting length"},
    { RunSettingsId::SPH_AV_ALPHA,                  "sph.av.alpha",             1.5_f,
        "Coefficient alpha_AV of the standard artificial viscosity." },
    { RunSettingsId::SPH_AV_BETA,                   "sph.av.beta",              3._f,
        "Coefficient beta_AV of the standard artificial viscosity. "},
    { RunSettingsId::SPH_AV_TYPE,                   "sph.av.type",              ArtificialViscosityEnum::STANDARD,
        "Type of the artificial viscosity used by the SPH solver. Can be one of the following:\n" + EnumMap::getDesc<ArtificialViscosityEnum>() },
    { RunSettingsId::SPH_AV_USE_BALSARA,            "sph.av.balsara.use",       false,
        "Specifies if the Balsara switch is used when computing artificial viscosity" },
    { RunSettingsId::SPH_AV_BALSARA_STORE,          "sph.av.balsara.store",     false,
        "Debug parameter; if true, Balsara coefficient is stored as a quantity and can be saved to output file. "},
    { RunSettingsId::SPH_AV_USE_STRESS,             "sph.av.stress.use",        false,
        "Whether to use artificial stress. "},
    { RunSettingsId::SPH_AV_STRESS_EXPONENT,        "sph.av.stress.exponent",   4._f,
        "Kernel exponent of the artificial stress. "},
    { RunSettingsId::SPH_AV_STRESS_FACTOR,          "sph.av.stress.factor",     0.04_f,
        "Multiplicative factor of the artificial stress. "},
    { RunSettingsId::SPH_USE_AC,                    "sph.ac.use",               false,
        "Enables artificial thermal conductivity term" },
    { RunSettingsId::SPH_AC_ALPHA,                  "sph.ac.alpha",             1._f,
        "Artificial conductivity alpha coefficient." },
    { RunSettingsId::SPH_AC_BETA,                   "sph.ac.beta",              1.5_f,
        "Artificial conductivity beta coefficient." },
    { RunSettingsId::SPH_AC_SIGNAL_SPEED, "sph.ac.signal_speed", SignalSpeedEnum::PRESSURE_DIFFERENCE,
        "Type of the signal speed used by artificial conductivity. Can be one of the following:\n" +  EnumMap::getDesc<SignalSpeedEnum>() },
    { RunSettingsId::SPH_SMOOTHING_LENGTH_MIN,      "sph.smoothing_length.min",         1e-5_f,
        "Minimal value of the smoothing length (in meters). "},
    { RunSettingsId::SPH_PHASE_ANGLE,               "sph.phase_angle",                  false,
        "If true, phase angle of each particle is saved in storage and evolved in time. "},
    { RunSettingsId::SPH_FINDER,                    "sph.finder",                       FinderEnum::KD_TREE,
        "Acceleration structure used for finding neighbors (Kn queries). Can be one of the following:\n" + EnumMap::getDesc<FinderEnum>() },
    { RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, "sph.correction_tensor",        false,
        "If true, correction tensor is applied on gradient when computing strain rate. Essential "
        "for correct simulation of rotating bodies." },
    { RunSettingsId::SPH_SUM_ONLY_UNDAMAGED,        "sph.sum_only_undamaged",       true,
        "If true, completely damaged particles (D=1) are excluded when computing strain rate and "
        "stress divergence. Solver also excludes particles of different bodies; when computing "
        "strain rate in target, particles in impactor are excluded from the sum." },
    { RunSettingsId::SPH_CONTINUITY_MODE,           "sph.continuity_mode",             ContinuityEnum::STANDARD,
        "Specifies how the density is evolved. Can be one of the following:\n" + EnumMap::getDesc<ContinuityEnum>() },
    { RunSettingsId::SPH_DISCRETIZATION,            "sph.discretization",              DiscretizationEnum::STANDARD,
        "Specifies a discretization of SPH equations. Can be one of the following:\n" + EnumMap::getDesc<DiscretizationEnum>() },
    { RunSettingsId::SPH_STABILIZATION_DAMPING,     "sph.stabilization_damping",       0.1_f,
        "Specifies the damping coefficient of particle velocities. This is mainly intended for stabilization phase, "
       "it should not be used in the main simulation." },

    /// Global parameters of N-body simulations
    { RunSettingsId::NBODY_INERTIA_TENSOR,          "nbody.inertia_tensor",     false,
        "If true, each particle has generally non-isotropic inertia tensor. The inertia tensor is evolved using "
        "Euler's equations. Particle geometry is still spherical though; particles always collide as two spheres "
        "and merge into a larger sphere, the inertia tensor is summed up using parallel axis theorem." },
    { RunSettingsId::NBODY_MAX_ROTATION_ANGLE,      "nbody.max_rotation_angle", 0.5_f,
        "Maximum angle of rotation in a single iteration. " },
    { RunSettingsId::NBODY_AGGREGATES_ENABLE,       "nbody.aggregates.enable",  false,
        "If true, colliding particles form aggregates, which then move and rotate as rigid bodies. There are no collisions "
        "between particles belonging to the same aggregate, only collisions of different aggregates need to be handled. Note "
        "that enabling aggregates overrides handlers of collisions and overlaps." },
    { RunSettingsId::NBODY_AGGREGATES_SOURCE,       "nbody.aggregates.source",  AggregateEnum::PARTICLES,
        "Specifies the initial aggregates used in the simulation. Can be one of the following:\n" + EnumMap::getDesc<AggregateEnum>() },

    /// Gravity
    { RunSettingsId::GRAVITY_SOLVER,                "gravity.solver",           GravityEnum::BARNES_HUT,
        "Solver for computing gravitational acceleration. Can be one of the following:\n" + EnumMap::getDesc<GravityEnum>() },
    { RunSettingsId::GRAVITY_OPENING_ANGLE,         "gravity.opening_angle",    0.5_f,
        "Opening angle (in radians) used in Barnes-Hut algorithm. Larger values means faster gravity evaluation "
        "at a cost of lower precision." },
    { RunSettingsId::GRAVITY_MULTIPOLE_ORDER,       "gravity.multipole_order",  3,
        "Maximum order of gravitational moments. Use 0 for monopole, 2 for quadrupole, etc. " },
    { RunSettingsId::GRAVITY_KERNEL,                "gravity.kernel",           GravityKernelEnum::SPH_KERNEL,
        "Smoothing kernel of gravity. Can be one of the following:\n" + EnumMap::getDesc<GravityKernelEnum>() },
    { RunSettingsId::GRAVITY_CONSTANT,              "gravity.constant",         Constants::gravity,
        "todo" },
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
    { RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT,       "collision.merging_limit",    1._f,
        "Multiplier of the relative velocity and the angular velocity of the merger, used when determining "
        "whether to merge the collided particles or reject the collision. If zero, particles are always merged, "
        "values slightly lower than 1 can be used to simulate strength, holding together a body rotating above "
        "the breakup limit. Larger values can be used to merge only very slowly moving particles." },
    { RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT,     "collision.rotation_merging_limit",  1._f,
        "Parameter analogous to collision.bounce_merge_limit, but used for the rotation of the merger. "
        "Particles can only be merged if the angular frequency multiplied by this parameter is lower than the "
        "breakup frequency. If zero, particles are always merged, values larger than 1 can be used to avoid "
        "fast rotators in the simulation." },


    /// Parameters of the soft-body solver
    { RunSettingsId::SOFT_REPEL_STRENGTH,       "soft.repel_strength",    1._f,
        "Repel strength used by the soft-body solver" },
    { RunSettingsId::SOFT_FRICTION_STRENGTH,    "soft.friction_strength",  0.01_f,
        "Friction strength used by the soft-body solver" },

    /// Timestepping parameters
    { RunSettingsId::TIMESTEPPING_INTEGRATOR,       "timestep.integrator",      TimesteppingEnum::PREDICTOR_CORRECTOR,
        "Integrator performing evolution in time. Can be one of the following:\n" + EnumMap::getDesc<TimesteppingEnum>() },
    { RunSettingsId::TIMESTEPPING_COURANT_NUMBER,   "timestep.courant_number",         0.2_f,
        "Courant number limiting the time step value. Needed for numerical stability of the integrator. Always keep <= 1!" },
    { RunSettingsId::TIMESTEPPING_MAX_TIMESTEP,     "timestep.max_step",        10._f,
        "Maximal allowed value of the time step." },
    { RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, "timestep.initial",         0.03_f,
        "Initial time step of the simulation. "},
    { RunSettingsId::TIMESTEPPING_CRITERION,        "timestep.criterion",       TimeStepCriterionEnum::ALL,
        "Criteria limiting the value of the time step. Can be one or more values from:\n" + EnumMap::getDesc<TimeStepCriterionEnum>() },
    { RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR,  "timestep.derivative_factor", 0.2_f,
        "Multiplicative factor of the time step computed as a value-to-derivative ratio of time-dependent quantities."},
    { RunSettingsId::TIMESTEPPING_DIVERGENCE_FACTOR,  "timestep.divergence_factor", 0.005_f,
        "Multiplicative factor of the time step computed using reciprocal velocity divergence."},
    { RunSettingsId::TIMESTEPPING_MEAN_POWER,       "timestep.mean_power",      -INFTY,
        "Power of the generalized mean, used to compute the final timestep from timesteps of individual "
        "particles. Negative infinity means the minimal timestep is used. This value will also set statistics "
        "of the restricting particle, namely the particle index and the quantity value and corresponding "
        "derivative of the particle; these statistics are not saved for other powers." },
    { RunSettingsId::TIMESTEPPING_MAX_INCREASE,     "timestep.max_change",      INFTY,
        "Maximum relative growth of the time steps in subsequent iterations. Used to 'smooth' the integration and "
        "to avoid rapid changes of time steps."},
    { RunSettingsId::TIMESTEPPING_MIDPOINT_COUNT,   "timestep.midpoint_count",  5,
        "Applicable for modified midpoint method. Specified the number of sub-steps within one time step." },
    { RunSettingsId::TIMESTEPPING_BS_ACCURACY,      "timestep.bs.accuracy",     1.e-3_f,
        "Required relative accuracy (epsilon value) of the Bulirsch-Stoer integrator." },
    { RunSettingsId::SAVE_PARTICLE_TIMESTEPS,       "save_particle_timesteps",  false,
        "If true, time steps determined for each particle are stored in the 'time step' quantity." },

    /// Kd-tree and other finders
    { RunSettingsId::FINDER_LEAF_SIZE,          "finder.leaf_size",             25,
        "Maximal number of particles in the leaf of K-d tree." },
    { RunSettingsId::FINDER_MAX_PARALLEL_DEPTH, "finder.max_parallel_depth", 50,
        "Maximal tree depth to be processed in parallel. A larger value implies better distribution of work "
        "between threads, but it also comes with performance penalty due to scheduling overhead." },

    /// Selected coordinate system, rotation of bodies
    { RunSettingsId::FRAME_ANGULAR_FREQUENCY,       "frame.angular_frequency",  Vector(0._f),
        "Used to perform the simulation in rotating (non-inertial) frame. Specifies a global rotation of the coordinate "
        "system around axis (0, 0, 1) passing through origin. If the solver includes inertial forces, rotating frame "
        "introduces centrifugal and Coriolis force." },
    { RunSettingsId::FRAME_CONSTANT_ACCELERATION,   "frame.constant_acceleration",  Vector(0._f),
        "Used to implement homogeneous gravity field." },
    { RunSettingsId::FRAME_TIDES_MASS,              "frame.tides.mass",             0._f,
        "." },
    { RunSettingsId::FRAME_TIDES_POSITION,          "frame.tides.position",         Vector(Constants::R_earth, 0._f, 0._f),
        "." },


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
        "Dimensions of the computational domain. Used by block and ellipsoidal domain." },

    /// Miscellaneous
    { RunSettingsId::GENERATE_UVWS,     "misc.generate_uvws",   false,
        "If true, the mapping coordinates will be generated and saved for all bodies in the simulation. "
        "Useful to visualize the simulation results with surface textures." },
    { RunSettingsId::UVW_MAPPING,       "misc.uvw_mapping",     UvMapEnum::SPHERICAL,
        "Type of the UV mapping" },
    });
    return instance;
}

static RegisterEnum<DistributionEnum> sDistribution({
    { DistributionEnum::HEXAGONAL, "hexagonal", "Hexagonally close packing" },
    { DistributionEnum::CUBIC, "cubic", "Cubic close packing (generally unstable, mainly for tests!)" },
    { DistributionEnum::RANDOM, "random", "Randomly distributed particles" },
    { DistributionEnum::DIEHL_ET_AL, "diehl_et_al", "Isotropic uniform distribution by Diehl et al. (2012)" },
    { DistributionEnum::STRATIFIED, "stratified", "Stratified distribution" },
    { DistributionEnum::PARAMETRIZED_SPIRALING, "parametrized_spiraling", "Parametrized spiraling scheme" },
});

static RegisterEnum<EosEnum> sEos({
    { EosEnum::NONE,
        "none",
        "No equation of state. Implies there is no pressure nor stress in the "
        "body, can be used to simulate "
        "dust interacting only by friction or gravity." },
    { EosEnum::IDEAL_GAS, "ideal_gas", "Equation of state for ideal gas." },
    { EosEnum::TAIT, "tait", "Tait equation of state for simulations of liquids." },
    { EosEnum::MIE_GRUNEISEN,
        "mie_gruneisen",
        "Mie-Gruneisen equation of state. Simple model for solids without any phase transitions." },
    { EosEnum::TILLOTSON, "tillotson", "Tillotson equation of stats." },
    { EosEnum::MURNAGHAN, "murnaghan", "Murnaghan equation of state." },
    { EosEnum::SIMPLIFIED_TILLOTSON, "simplified_tillotson", "Simplified version of the Tillotson equation."},
    { EosEnum::ANEOS,
        "aneos",
        "ANEOS equation of state, requires look-up table of values for given material." },
});


template<>
const BodySettings& getDefaultSettings() {
    static BodySettings instance({
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
    { BodySettingsId::USE_ACOUSTIC_FLUDIZATION, "rheology.acoustic_fludization", false,
        "Whether to use the model of acoustic fludization." },
    { BodySettingsId::OSCILLATION_DECAY_TIME,   "rheology.oscillation_decay_time", 100._f,
        "Characteristic decay time of the acoustic oscillations in the material." },
    { BodySettingsId::OSCILLATION_REGENERATION, "rheology.oscillation_regeneration", 0.5_f,
        "Regeneration efficiency of the acoustic oscillations." },
    { BodySettingsId::FLUIDIZATION_VISCOSITY,   "rheology.fludization_viscosity", 8.8e4_f, // Ivanov and Turtle, 2001
        "Effective kinematic viscosity of acoustic fludization." },

    /// Material properties
    { BodySettingsId::DENSITY,                 "material.density",             2700._f,
        "Initial density of the material." },
    { BodySettingsId::DENSITY_RANGE,           "material.density.range",       Interval(50._f, INFTY),
        "Allowed range of densities for this material." },
    { BodySettingsId::DENSITY_MIN,             "material.density.min",         100._f,
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
    { BodySettingsId::WEIBULL_COEFFICIENT,     "material.weibull.coefficient", 4.e35_f,
        "Coefficient k of Weibull distribution." },
    { BodySettingsId::WEIBULL_EXPONENT,        "material.weibull.exponent",    9._f,
        "Coefficienet m of Weibull distribution." },
    { BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS, "material.weibull.sample_distributions", false,
        "If true, flaw counts and activation thresholds are samples from Poisson and exponential distribution, "
        "respectively. Otherwise, flaws and activation thresholds are accumulated for each particle until all "
        "particles have at least one flaw, as in Benz and Asphaug (1994). The latter is needed for reproducibility "
        "of older results and compatibility with SPH5, otherwise the distribution sampling should be used, "
        "especially for simulation with large (N > 1e7) number of particles." },
    { BodySettingsId::DISTENTION,              "material.palpha.distention",   1.275_f,
        "Initial value of the material distention, used in the P-alpha model." },
    { BodySettingsId::BULK_VISCOSITY,          "material.bulk_viscosity",      1.e20_f,
        "Bulk viscosity of the material. Applicable is internal friction is used." },
    { BodySettingsId::SHEAR_VISCOSITY,         "material.shear_viscosity",     1.e20_f,
        "Shear viscosity of the material. Applicable is internal friction is used." },
    { BodySettingsId::DIFFUSIVITY,             "material.diffusivity",         2.3e-5_f, // iron
        "Diffusivity of the material, used in heat diffusion equation." },
    { BodySettingsId::SURFACE_TENSION,         "material.surface_tension",     1._f,
        "Surface tension of the fluid. Not applicable for solids nor gass." },
    { BodySettingsId::BULK_POROSITY,           "material.bulk_porosity",       0.4_f,
        "Bulk (macro)porosity of the material, used when creating a rubble-pile body" },
    { BodySettingsId::HEAT_CAPACITY,           "material.heat_capacity",       700._f,
        "Specific heat capacity at constant pressure. While it is generally a function of temperature, "
        "this value can be used to estimate the temperature from the internal energy." },

    /// Particle distribution
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
    { BodySettingsId::SMOOTHING_LENGTH_ETA,    "sph.eta",                       1.3_f,
        "Multiplier of the kernel radius. Lower values means the particles are more localized (better spatial resolution), "
        "but they also have fewer neighbors, so the derivatives are evaluated with lower precision. Values between 1 and 2 "
        "should be used." },
    { BodySettingsId::DIEHL_STRENGTH,          "sph.diehl.strength",            0.1_f,
        "Magnitude of the particle displacement in a single iteration. Used by Diehl's distribution." },
    { BodySettingsId::DIEHL_MAX_DIFFERENCE,    "sph.diehl.max_difference",      10,
        "Maximum allowed difference between the expected number of particles and the actual number of generated "
        "particles. Higher value speed up the generation of particle positions." },
    { BodySettingsId::DIEHL_ITERATION_COUNT,    "sph.diehl.iteration_count",    50,
        "Number of iterations used to converge particles to their final positions. Higher value means more uniform "
        "and isotropic distribution, but also longer computation time." },
    { BodySettingsId::PARTICLE_COUNT,           "particles.count",              10000,
        "Required number of particles in the body. Note that the actual number of particles may differ, depending "
        "on the selected distribution. " },
    { BodySettingsId::MIN_PARTICLE_COUNT,       "particles.min_count",          100,
         "Minimal number of particles per one body. Used when creating 'sub-bodies' withing one 'parent' body, "
         "for example when creating rubble-pile asteroids, ice blocks inside an asteroid, etc. Parameter has no "
         "effect for creation of a single monolithic body; the number of particles from PARTICLE_COUNT is used "
         "in any case." },
    { BodySettingsId::AV_ALPHA,                 "av.alpha",                     1.5_f,
        "Initial coefficient alpha of the Morris-Monaghan artificial viscosity. Beta coefficient of the viscosity "
        "is derived as 2*alpha." },
    { BodySettingsId::AV_ALPHA_RANGE,           "av.alpha.range",               Interval(0.05_f, 1.5_f),
        "Allowed range of the alpha coefficient. Used by Morris-Monaghan artificial viscosity."},
    { BodySettingsId::BODY_CENTER,              "body.center",                  Vector(0._f),
        "Center of the body." },
    { BodySettingsId::BODY_SHAPE_TYPE,          "body.shape_type",              DomainEnum::SPHERICAL,
         "Can be one of the following:\n" + EnumMap::getDesc<DomainEnum>() },
    { BodySettingsId::BODY_RADIUS,              "body.radius",                  5.e3_f,
         "Radius of a spherical body" },
    { BodySettingsId::BODY_DIMENSIONS,          "body.dimensions",              Vector(10.e3_f),
         "Dimensions of a body." },
    { BodySettingsId::BODY_HEIGHT,              "body.height",                  10.e3_f,
         "Height of a body." },
    { BodySettingsId::BODY_SPIN_RATE,           "body.spin_rate",               0._f,
         "Spin rate of the body along z-axis in units rev/day." },

    /// Visualization
    { BodySettingsId::VISUALIZATION_TEXTURE,    "visualization.texture",        ""_s,
         "Path to the texture used by a renderer. May be empty." },

    /// Metadata
    { BodySettingsId::IDENTIFIER, "identifier", "basalt"_s,
      "Arbitrary string identifying this material" },
    });
    return instance;
}
// clang-format on


// Explicit instantiation
template class Settings<BodySettingsId>;
template class SettingsIterator<BodySettingsId>;

template class Settings<RunSettingsId>;
template class SettingsIterator<RunSettingsId>;

NAMESPACE_SPH_END
