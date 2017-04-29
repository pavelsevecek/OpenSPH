#pragma once

/// Generic storage and input/output routines of settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/TracelessTensor.h"
#include "objects/wrappers/Range.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"
#include <map>

NAMESPACE_SPH_BEGIN

class Outcome;

/// Tag for initialization of empty settings object.
struct EmptySettingsTag {};

const EmptySettingsTag EMPTY_SETTINGS;

/// Generic object containing various settings and parameters of the run.
template <typename TEnum>
class Settings {
private:
    enum Types { BOOL, INT, FLOAT, RANGE, STRING, VECTOR, TENSOR, TRACELESS_TENSOR };

    using Value = Variant<bool, int, Float, Range, std::string, Vector, Tensor, TracelessTensor>;

    struct Entry {
        TEnum id;
        std::string name;
        Value value;
    };

    std::map<TEnum, Entry> entries;

    static std::unique_ptr<Settings> instance;

public:
    /// Initialize settings by settings all value to their defaults.
    Settings()
        : Settings(Settings::getDefaults()) {}

    /// Initialize empty settings object.
    Settings(EmptySettingsTag) {}

    Settings(std::initializer_list<Entry> list) {
        for (auto&& entry : list) {
            entries[entry.id] = entry;
        }
    }

    /// Assigns a list of settings into the object, erasing all previous entries.
    Settings& operator=(std::initializer_list<Entry> list) {
        entries.clear();
        for (auto&& entry : list) {
            entries[entry.id] = entry;
        }
        return *this;
    }

    /// Saves a value into the settings. Any previous value of the same ID is overriden.
    /// \returns Reference to the settings object, allowing to queue multiple set functions.
    template <typename TValue>
    Settings& set(const TEnum idx, TValue&& value) {
        using StoreType = ConvertToSize<TValue>;
        entries[idx].value = StoreType(std::forward<TValue>(value));
        return *this;
    }

    /// Returns a value of given type from the settings. Value must be stored in settings and must have
    /// corresponding type, checked by assert.
    template <typename TValue>
    TValue get(const TEnum idx) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        ASSERT(iter != entries.end());
        using StoreType = ConvertToSize<TValue>;
        const StoreType& value = iter->second.value.template get<StoreType>();
        return TValue(value);
    }

    /// Copies a value stored in this settings to another settings object. Value of the same ID stored in
    /// destination object is overriden. Value of given ID must be stored in this settings, checked by assert.
    /// Works independently of value's type.
    void copyValueTo(const TEnum idx, Settings& dest) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        ASSERT(iter != entries.end());
        dest.entries[idx].value = iter->second.value;
    }

    /// Saves all values stored in settings into file.
    void saveToFile(const std::string& path) const;

    /// \todo split settings and descriptors? Settings actual object with values, descriptors global object
    /// with ids, names and default values.
    Outcome loadFromFile(const std::string& path, const Settings& descriptors);

    /// Returns a reference to object containing default values of all settings.
    static const Settings& getDefaults();

private:
    bool setValueByType(Entry& entry, const Size typeIdx, const std::string& str);
};

enum class KernelEnum {
    /// M4 B-spline (piecewise cubic polynomial)
    CUBIC_SPLINE,

    /// M5 B-spline (piecewise 4th-order polynomial)
    FOURTH_ORDER_SPLINE,

    /// Gaussian function
    GAUSSIAN,

    /// Core Triangle (CT) kernel by Read et al. (2010)
    CORE_TRIANGLE
};

enum class TimesteppingEnum {
    /// Explicit (forward) 1st-order integration
    EULER_EXPLICIT,

    /// Leap-frog 2nd-order integration
    LEAP_FROG,

    /// Runge-Kutta 4-th order integration
    RUNGE_KUTTA,

    /// Predictor-corrector scheme
    PREDICTOR_CORRECTOR,

    /// Bulirsch-Stoer integrator
    BULIRSCH_STOER
};

enum class TimeStepCriterionEnum {
    /// Constant time step, determined by initial value
    NONE = 0,

    /// Time step determined using CFL condition
    COURANT = 1 << 1,

    /// Time step computed by limiting value-to-derivative ratio of quantiites.
    DERIVATIVES = 1 << 2,

    /// Time step computed from ratio of acceleration and smoothing length.
    ACCELERATION = 1 << 3,

    /// Value for using all criteria.
    ALL = COURANT | DERIVATIVES | ACCELERATION,
};

enum class FinderEnum {
    /// Brute-force search by going through each pair of particles (O(N^2) complexity)
    BRUTE_FORCE,

    /// Using K-d tree
    KD_TREE,

    /// Using octree
    OCTREE,

    /// Using linked list
    LINKED_LIST,

    /// Partitioning particles into voxels
    VOXEL
};

enum class BoundaryEnum {
    /// Do not use any boundary conditions (= vacuum conditions)
    NONE,

    /// Highest derivatives of all particles close to the boundary are set to zero.
    FROZEN_PARTICLES,

    /// Create ghosts to keep particles inside domain
    GHOST_PARTICLES,

    /// Extension of Frozen Particles, pushing particles inside the domain and removing them on the other end.
    WIND_TUNNEL,

    /// Periodic boundary conditions
    PERIODIC,

    /// Project all movement onto a line, effectivelly reducing the simulation to 1D
    PROJECT_1D
};

enum class DomainEnum {
    /// No computational domain (can only be used with BoundaryEnum::NONE)
    NONE,

    /// Sphere with given radius
    SPHERICAL,

    /// Block with edge sizes given by vector
    BLOCK,

    /// Cylindrical domain aligned with z axis
    CYLINDER
};

enum class ArtificialViscosityEnum {
    /// No artificial viscosity
    NONE,

    /// Standard artificial viscosity term by Monaghan (1989).
    STANDARD,

    /// Artificial viscosity term analogous to Riemann solvers by Monaghan (1997)
    RIEMANN,

    /// Time-dependent artificial viscosity by Morris & Monaghan (1997).
    MORRIS_MONAGHAN,
};

enum class SolverEnum {
    /// Standard SPH formulation evolving density, velocity and internal energy in time.
    CONTINUITY_SOLVER,

    /// Density is obtained by direct summation over nearest SPH particles.
    SUMMATION_SOLVER,

    /// Density independent solver by Saitoh & Makino (2013).
    DENSITY_INDEPENDENT
};

enum class YieldingEnum {
    /// No yielding, just elastic deformations following Hooke's law
    NONE,

    /// Von Mises criterion
    VON_MISES,

    /// Drucker-Prager pressure dependent yielding stress
    DRUCKER_PRAGER
};

enum class DamageEnum {
    /// No fragmentation
    NONE,

    /// Grady-Kipp model of fragmentation using scalar damage
    SCALAR_GRADY_KIPP,

    /// Grady-Kipp model of fragmentation using tensor damage
    TENSOR_GRADY_KIPP
};

enum class SmoothingLengthEnum {
    /// Smoothing length is constant and given by initial conditions
    CONST = 1 << 0,

    /// Smoothing length is evolved using continuity equation
    CONTINUITY_EQUATION = 1 << 1,

    /// Number of neighbours is kept fixed by adding additional derivatives of smoothing length, scaled by
    /// local sound speed
    SOUND_SPEED_ENFORCING = 1 << 2
};

enum class LoggerEnum {
    /// Do not log anything
    NONE,

    /// Print log to standard output
    STD_OUT,

    /// Print log to file
    FILE

    /// \todo print using callback to gui application
};

enum class OutputEnum {
    /// No output
    NONE,

    /// Save output data into formatted human-readable text file
    TEXT_FILE,

    /// Save output data into binary file. This data dump is lossless and can be use to restart run from saved
    /// snapshot.
    BINARY_FILE,
};

enum class RngEnum {
    /// Mersenne Twister PRNG from Standard library
    UNIFORM,

    /// Halton QRNG
    HALTON,

    /// Same RNG as used in SPH5, used for 1-1 comparison
    BENZ_ASPHAUG
};

/// Settings relevant for whole run of the simulation
enum class RunSettingsId {
    /// Custom name of the run
    RUN_NAME,

    /// Selected format of the output file, see OutputEnum
    RUN_OUTPUT_TYPE,

    /// Time interval of dumping data to disk.
    RUN_OUTPUT_INTERVAL,

    /// File name of the output file (including extension), where %d is a placeholder for output number.
    RUN_OUTPUT_NAME,

    /// Path where all output files (dumps, logs, ...) will be written
    RUN_OUTPUT_PATH,

    /// Number of threads used by the code. If 0, all available threads are used.
    RUN_THREAD_CNT,

    /// Number of particles processed by one thread in a single batch. Lower number can help to distribute
    /// tasks between threads more evenly, higher number means faster processing of particles within single
    /// thread.
    RUN_THREAD_GRANULARITY,

    /// Selected logger of a run, see LoggerEnum
    RUN_LOGGER,

    /// Path of a file where the log is printed, used only when selected logger is LoggerEnum::FILE
    RUN_LOGGER_FILE,

    /// Frequency of statistics evaluation
    RUN_STATISTICS_STEP,

    /// Starting time and ending time of the run. Run does not necessarily have to start at t = 0.
    RUN_TIME_RANGE,

    /// Maximum number of timesteps after which run ends. 0 means run duration is not limited by number of
    /// timesteps. Note that if adaptive timestepping is used, run can end at different times for
    /// different initial conditions. This condition should only be used for debugging purposes.
    RUN_TIMESTEP_CNT,

    /// Maximum duration of the run in milliseconds, measured in real-world time. 0 means run duration is not
    /// limited by this value.
    RUN_WALLCLOCK_TIME,

    /// Selected random-number generator used within the run.
    RUN_RNG,

    /// Seed for the random-number generator
    RUN_RNG_SEED,

    /// Index of SPH Kernel, see KernelEnum
    SPH_KERNEL,

    /// Structure for searching nearest neighbours of particles
    SPH_FINDER,

    /// If true, the kernel gradient will be corrected for each particle to improve conservation of total
    /// angular momentum. This comes at the cost of higher memory consumption and slower evaluation of SPH
    /// derivatives.
    SPH_CONSERVE_ANGULAR_MOMENTUM,

    /// Eta-factor between smoothing length and particle concentration (h = eta * n^(-1/d) )
    SPH_KERNEL_ETA,

    /// Minimum and maximum number of neighbours SPH solver tries to enforce. Note that the solver cannot
    /// guarantee the actual number of neighbours will be within the range.
    SPH_NEIGHBOUR_RANGE,

    /// Strength of enforcing neighbour number. Higher value makes enforcing more strict (number of neighbours
    /// gets into required range faster), but also makes code less stable. Can be a negative number, -INFTY
    /// technically disables enforcing altogether.
    SPH_NEIGHBOUR_ENFORCING,

    /// Artificial viscosity alpha coefficient
    SPH_AV_ALPHA,

    /// Artificial viscosity beta coefficient
    SPH_AV_BETA,

    /// Minimal value of smoothing length
    SPH_SMOOTHING_LENGTH_MIN,

    /// Use force from pressure gradient in the model
    MODEL_FORCE_PRESSURE_GRADIENT,

    /// Use force from stress divergence in the model (must be used together with MODEL_FORCE_GRAD_P). Stress
    /// tensor is then evolved in time using Hooke's equation.
    MODEL_FORCE_SOLID_STRESS,

    MODEL_FORCE_NAVIER_STOKES,

    /// Use centripetal force given by angular frequency of the coordinate frame in the model
    MODEL_FORCE_CENTRIPETAL,

    /// Use gravitational force in the model.
    MODEL_FORCE_GRAVITY,

    /// Type of used artificial viscosity.
    MODEL_AV_TYPE,

    /// Whether to use balsara switch for computing artificial viscosity dissipation. If no artificial
    /// viscosity is used, the value has no effect.
    MODEL_AV_BALSARA,

    /// If true, Balsara factors will be saved as quantity AV_BALSARA. Mainly for debugging purposes.
    MODEL_AV_BALSARA_STORE,

    /// Selected solver for computing derivatives of physical variables.
    SOLVER_TYPE,

    /// Solution for evolutions of the smoothing length
    ADAPTIVE_SMOOTHING_LENGTH,

    /// Number of spatial dimensions of the problem.
    SOLVER_DIMENSIONS,

    /// Maximum number of iterations for self-consistent density computation of summation solver.
    SUMMATION_MAX_ITERATIONS,

    /// Target relative difference of density in successive iterations. Density computation in summation
    /// solver is ended when the density changes by less than the delta or the iteration number exceeds
    /// SOLVER_SUMMATION_MAX_ITERATIONS.
    SUMMATION_DENSITY_DELTA,

    /// Selected timestepping integrator
    TIMESTEPPING_INTEGRATOR,

    /// Courant number
    TIMESTEPPING_COURANT,

    /// Upper limit of the time step. The timestep is guaranteed to never exceed this value for any timestep
    /// criterion. The lowest possible timestep is not set, timestep can be any positive value.
    TIMESTEPPING_MAX_TIMESTEP,

    /// Initial value of time step. If dynamic timestep is disabled, the run will keep the initial timestep
    /// for the whole duration. Some timestepping algorithms might not use the initial timestep and directly
    /// compute new value of timestep, in which case this parameter has no effect.
    TIMESTEPPING_INITIAL_TIMESTEP,

    /// Criterion used to determine value of time step. More criteria may be compined, in which case the
    /// smallest time step of all is selected.
    TIMESTEPPING_CRITERION,

    /// Multiplicative factor k in timestep computation; dt = k * v / dv
    TIMESTEPPING_ADAPTIVE_FACTOR,

    /// Global rotation of the coordinate system around axis (0, 0, 1) passing through origin. If non-zero,
    /// causes non-intertial acceleration.
    FRAME_ANGULAR_FREQUENCY,

    /// Computational domain, enforced by boundary conditions
    DOMAIN_TYPE,

    /// Type of boundary conditions.
    DOMAIN_BOUNDARY,

    /// Center point of the domain
    DOMAIN_CENTER,

    /// Radius of a spherical and cylindrical domain
    DOMAIN_RADIUS,

    /// Height of a cylindrical domain
    DOMAIN_HEIGHT,

    /// (Vector) size of a block domain
    DOMAIN_SIZE,

    /// Minimal distance between a particle and its ghost, in units of smoothing length.
    DOMAIN_GHOST_MIN_DIST,

    /// Distance to the boundary in units of smoothing length under which the particles are frozen.
    DOMAIN_FROZEN_DIST,
};


enum class DistributionEnum {
    /// Hexagonally close packing
    HEXAGONAL,

    /// Cubic close packing
    CUBIC,

    /// Random distribution of particles
    RANDOM,

    /// Isotropic uniform distribution by Diehl et al. (2012)
    DIEHL_ET_AL,

    /// Distributes particles uniformly on line
    LINEAR
};


enum class EosEnum {
    /// No equation of stats
    NONE,

    /// Equation of state for ideal gas
    IDEAL_GAS,

    /// Tillotson (1962) equation of state
    TILLOTSON,

    /// Murnaghan equation of state
    MURNAGHAN,

    /// ANEOS given by look-up table
    ANEOS
};

/// Settings of a single body / gas phase / ...
/// Combines material parameters and numerical parameters of the SPH method specific for one body.
enum class BodySettingsId {
    /// Equation of state for this material, see EosEnum for options.
    EOS,

    /// Initial distribution of SPH particles within the domain, see DistributionEnum for options.
    INITIAL_DISTRIBUTION,

    /// If true, generated particles will be moved so that their center of mass corresponds to the center of
    /// selected domain. Note that this will potentially move some particles outside of the domain, which can
    /// clash with boundary conditions.
    CENTER_PARTICLES,

    /// If true, particles are sorted using Morton code, preserving locality in memory.
    PARTICLE_SORTING,

    /// Turns on 'SPH5 compatibility' mode when generating particle positions. This allows 1-1 comparison of
    /// generated arrays, but results in too many generated particles (by about factor 1.4). The option also
    /// implies CENTER_PARTICLES.
    DISTRIBUTE_MODE_SPH5,

    /// Density at zero pressure
    DENSITY,

    /// Allowed range of density. Densities of all particles all clamped to fit in the range.
    DENSITY_RANGE,

    /// Estimated minimal value of density. This value is NOT used to clamp densities, but for
    /// determining error of timestepping.
    DENSITY_MIN,

    /// Initial specific internal energy
    ENERGY,

    /// Allowed range of specific internal energy.
    ENERGY_RANGE,

    /// Estimated minimal value of energy used to determine timestepping error.
    ENERGY_MIN,

    /// Initial values of the deviatoric stress tensor
    STRESS_TENSOR,

    /// Estimated minial value of stress tensor components used to determined timestepping error.
    STRESS_TENSOR_MIN,

    /// Initial damage of the body.
    DAMAGE,

    /// Allowed range of damage.
    DAMAGE_RANGE,

    /// Estimate minimal value of damage used to determine timestepping error.
    DAMAGE_MIN,

    /// Adiabatic index used by some equations of state (such as ideal gas)
    ADIABATIC_INDEX,

    /// Bulk modulus of the material
    BULK_MODULUS,

    /// Coefficient B of the nonlinear compressive term in Tillotson equation
    TILLOTSON_NONLINEAR_B,

    /// "Small a" coefficient in Tillotson equation
    TILLOTSON_SMALL_A,

    /// "Small b" coefficient in Tillotson equation
    TILLOTSON_SMALL_B,

    /// Alpha coefficient in expanded phase of Tillotson equation
    TILLOTSON_ALPHA,

    /// Beta coefficient in expanded phase of Tillotson equation
    TILLOTSON_BETA,

    /// Specific sublimation energy
    TILLOTSON_SUBLIMATION,

    /// Specific energy of incipient vaporization
    TILLOTSON_ENERGY_IV,

    /// Specific energy of complete vaporization
    TILLOTSON_ENERGY_CV,

    RHEOLOGY_YIELDING,

    RHEOLOGY_DAMAGE,

    SHEAR_MODULUS,

    YOUNG_MODULUS,

    /// Elasticity limit of the von Mises yielding criterion
    ELASTICITY_LIMIT,

    MELT_ENERGY,

    /// Cohesion, yield strength at zero pressure
    COHESION,

    /// Coefficient of friction for undamaged material
    INTERNAL_FRICTION,

    /// Coefficient of friction for fully damaged material
    DRY_FRICTION,

    /// Speed of crack growth, in units of local sound speed.
    RAYLEIGH_SOUND_SPEED,

    WEIBULL_COEFFICIENT,

    WEIBULL_EXPONENT,

    /// Number of SPH particles in the body
    PARTICLE_COUNT,

    /// Initial alpha coefficient of the artificial viscosity. This is only used if the coefficient is
    /// different for each particle. For constant coefficient shared for all particles, use value from global
    /// settings.
    AV_ALPHA,

    /// Lower and upper bound of the alpha coefficient, used only for time-dependent artificial viscosity.
    AV_ALPHA_RANGE,

    /// Initial beta coefficient of the artificial viscosity.
    AV_BETA,

    /// Lower and upper bound of the alpha coefficient, used only for time-dependent artificial viscosity.
    AV_BETA_RANGE,
};

using RunSettings = Settings<RunSettingsId>;
using BodySettings = Settings<BodySettingsId>;

NAMESPACE_SPH_END
