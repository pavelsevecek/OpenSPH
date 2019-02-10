#pragma once

/// \file Settings.h
/// \brief Generic storage and input/output routines of settings.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/geometry/TracelessTensor.h"
#include "objects/utility/EnumMap.h"
#include "objects/wrappers/ClonePtr.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/Outcome.h"
#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN

class Path;

template <typename TEnum>
class SettingsIterator;

/// Tag for initialization of empty settings object.
struct EmptySettingsTag {};

const EmptySettingsTag EMPTY_SETTINGS;

/// \brief Wrapper of an enum.
///
/// Used to store an enum in settings while keeping (to some degree) the type safety.
struct EnumWrapper {
    int value;
    std::size_t typeHash;

    EnumWrapper() = default;

    template <typename TEnum>
    explicit EnumWrapper(TEnum e)
        : value(int(e))
        , typeHash(typeid(TEnum).hash_code()) {
        static_assert(std::is_enum<TEnum>::value, "Can be used only for enums");
    }

    EnumWrapper(const int value, const std::size_t hash)
        : value(value)
        , typeHash(hash) {}

    explicit operator int() const {
        return value;
    }

    bool operator==(const EnumWrapper& other) const {
        return value == other.value && typeHash == other.typeHash;
    }

    friend std::ostream& operator<<(std::ostream& ofs, const EnumWrapper& e) {
        ofs << e.value << " (" << e.typeHash << ")";
        return ofs;
    }
};

/// \brief Generic object containing various settings and parameters of the run.
///
/// Settings is a storage containing pairs key-value objects, where key is one of predefined enums. The value
/// can have multiple types within the same \ref Settings object. Currently following types can be stored:
/// bool, int, enums, float (or double), std::string, \ref Interval, \ref Vector, \ref SymmetricTensor, \ref
/// TracelessTensor.
///
/// The template cannot be used directly as it is missing default values of parameters; instead
/// specializations for specific enums should be used. The code defines two specializations:
///     - \ref BodySettings (specialization with enum \ref BodySettingsId)
///     - \ref RunSettings (specialization with enum \ref RunSettingsId)
///
/// The object can be specialized for other usages, provided static member \ref Settings::instance is created,
/// see one of existing specializations.
template <typename TEnum>
class Settings {
    template <typename>
    friend class SettingsIterator;

private:
    /// \brief List of types that can be stored in settings
    enum Types {
        BOOL,
        INT,
        FLOAT,
        INTERVAL,
        STRING,
        VECTOR,
        SYMMETRIC_TENSOR,
        TRACELESS_TENSOR,
        ENUM,
    };

    /// \brief Storage type of settings entries.
    ///
    /// Order of types in the variant must correspond to the values in enum \ref Types. It is possible to add
    /// other types to the settings, but always to the END of the variant to keep the backwards compatibility
    /// of serializer.
    ///
    /// \todo Possibly refactor by using some polymorphic holder (Any-type) rather than variant, this will
    /// allow to add more types for other Settings specializations (GuiSettings, etc.)
    using Value = Variant<bool,
        int,
        Float,
        Interval,
        std::string,
        Vector,
        SymmetricTensor,
        TracelessTensor,
        EnumWrapper>;

    struct Entry {
        /// Index of the property
        TEnum id;

        /// Unique text identifier of the property
        std::string name;

        /// Current value
        Value value;

        /// Description of the property. Can be empty.
        std::string desc;

        Entry() = default;

        Entry(TEnum id, const std::string& name, const Value& value, const std::string& desc = "")
            : id(id)
            , name(name)
            , value(value)
            , desc(desc) {}

        template <typename T, typename = std::enable_if_t<std::is_enum<T>::value>>
        Entry(TEnum id, const std::string& name, const T& value, const std::string& desc = "")
            : id(id)
            , name(name)
            , value(EnumWrapper(value))
            , desc(desc) {}

        template <typename T>
        Entry(TEnum id, const std::string& name, Flags<T> flags, const std::string& desc = "")
            : id(id)
            , name(name)
            , value(EnumWrapper(T(flags.value())))
            , desc(desc) {}
    };

    FlatMap<TEnum, Entry> entries;

    static AutoPtr<Settings> instance;

    /// Constructs settings from list of key-value pairs.
    Settings(std::initializer_list<Entry> list);

public:
    /// \brief Initialize settings by settings all value to their defaults.
    Settings();

    /// \brief Initialize empty settings object.
    Settings(EmptySettingsTag);

    Settings(const Settings& other);

    Settings(Settings&& other);

    /// \brief Assigns a list of settings into the object, erasing all previous entries.
    Settings& operator=(std::initializer_list<Entry> list);

    Settings& operator=(const Settings& other);

    Settings& operator=(Settings&& other);

    /// \brief Saves a value into the settings.
    ///
    /// Any previous value of the same ID is overriden.
    /// \tparam TValue Type of the value to be saved. Does not have to be specified, type deduction can be
    ///                used to determine it. Must be one of types listed in object description. Using other
    ///                types will result in compile error.
    /// \param idx Key identifying the value. This key can be used to retrive the value later.
    /// \param value Value being stored into settings.
    /// \returns Reference to the settings object, allowing to queue multiple set functions.
    template <typename TValue>
    Settings& set(const TEnum idx,
        TValue&& value,
        std::enable_if_t<!std::is_enum<std::decay_t<TValue>>::value, int> = 0) {
        // either the values is new or the type is the same as the previous value
        ASSERT(!entries.contains(idx) || entries[idx].value.template has<std::decay_t<TValue>>());
        if (!entries.contains(idx)) {
            entries.insert(idx, Entry{});
        }
        entries[idx].value = std::forward<TValue>(value);
        return *this;
    }

    /// \copydoc set
    template <typename TValue>
    Settings& set(const TEnum idx,
        TValue&& value,
        std::enable_if_t<std::is_enum<std::decay_t<TValue>>::value, int> = 0) {
        // either the values is new or the type is the same as the previous value
        ASSERT(!entries.contains(idx) || entries[idx].value.template has<EnumWrapper>());
        if (!entries.contains(idx)) {
            entries.insert(idx, Entry{});
        }
        entries[idx].value = EnumWrapper(value);
        return *this;
    }

    /// \brief Saves flags into the settings.
    ///
    /// This is internally saved as integer. There is no type-checking, flags can be freely converted to the
    /// underlying enum or even int using function \ref setFlags and \ref getFlags.
    template <typename TValue, typename = std::enable_if_t<std::is_enum<TValue>::value>>
    Settings& set(const TEnum idx, const Flags<TValue> flags) {
        ASSERT(!entries.contains(idx) || entries[idx].value.template has<EnumWrapper>());
        if (!entries.contains(idx)) {
            entries.insert(idx, Entry{});
        }
        entries[idx].value = EnumWrapper(TValue(flags.value()));
        return *this;
    }

    /// \brief Clear flags of given parameter in settings.
    Settings& set(const TEnum idx, EmptyFlags) {
        // can be used only if the value is already there and we know the type
        EnumWrapper currentValue = entries[idx].value.template get<EnumWrapper>();
        if (!entries.contains(idx)) {
            entries.insert(idx, Entry{});
        }
        entries[idx].value = EnumWrapper(0, currentValue.typeHash);
        return *this;
    }

    /// \brief todo
    Settings& set(const TEnum idx, const EnumWrapper ew) {
#ifdef SPH_DEBUG
        if (entries.contains(idx)) {
            const EnumWrapper current = entries[idx].value.template get<EnumWrapper>();
            ASSERT(current.typeHash == ew.typeHash, current.typeHash, ew.typeHash);
        }
#endif
        if (!entries.contains(idx)) {
            entries.insert(idx, Entry{});
        }
        entries[idx].value = ew;
        return *this;
    }

    /// \brief Adds entries from different \ref Settings object into this one, overriding current entries.
    ///
    /// Entries not stored in the given settings are kept unchanged.
    void addEntries(const Settings& settings) {
        for (const typename SettingsIterator<TEnum>::IteratorValue& iv : settings) {
            if (!entries.contains(iv.id)) {
                entries.insert(iv.id, getDefaults().entries[iv.id]);
            }
            entries[iv.id].value = iv.value;
        }
    }

    /// \brief Removes given parameter from settings.
    ///
    /// If the parameter is not in settings, nothing happens (it isn't considered as an error).
    void unset(const TEnum idx) {
        entries.tryRemove(idx);
    }

    /// \brief Returns a value of given type from the settings.
    ///
    /// Value must be stored in settings and must have corresponding type, checked by assert.
    /// \tparam TValue Type of the value we wish to return. This type must match the type of the saved
    ///                quantity.
    /// \param idx Key of the value.
    /// \returns Value correponsing to given key.
    template <typename TValue>
    TValue get(const TEnum idx, std::enable_if_t<!std::is_enum<std::decay_t<TValue>>::value, int> = 0) const {
        Optional<const Entry&> entry = entries.tryGet(idx);
        ASSERT(entry, int(idx));
        return entry->value.template get<TValue>();
    }

    template <typename TValue>
    TValue get(const TEnum idx, std::enable_if_t<std::is_enum<std::decay_t<TValue>>::value, int> = 0) const {
        Optional<const Entry&> entry = entries.tryGet(idx);
        ASSERT(entry, int(idx));
        EnumWrapper wrapper = entry->value.template get<EnumWrapper>();
        ASSERT(wrapper.typeHash == typeid(TValue).hash_code());
        return TValue(wrapper.value);
    }

    /// \brief Returns Flags from underlying value stored in settings.
    ///
    /// Syntactic suggar, avoid cumbersome conversion to underlying type and then to Flags.
    template <typename TValue>
    Flags<TValue> getFlags(const TEnum idx) const {
        static_assert(std::is_enum<TValue>::value, "Can be only used for enums");
        TValue value = this->get<TValue>(idx);
        return Flags<TValue>::fromValue(std::underlying_type_t<TValue>(value));
    }

    /// \brief Checks if the given entry is stored in the settings.
    bool has(const TEnum idx) const {
        return entries.contains(idx);
    }

    /// \brief Checks if the given entry has specified type.
    ///
    /// Entry must be in settings, checked by assert.
    template <typename TValue>
    bool hasType(const TEnum idx) const {
        Optional<const Entry&> entry = entries.tryGet(idx);
        ASSERT(entry, int(idx));
        return entry->value.template has<TValue>();
    }

    /// \brief Saves all values stored in settings into file.
    ///
    /// \param path Path (relative or absolute) to the file. The file will be created, any previous
    ///             content will be overriden.
    /// \returns SUCCESS if the file has been correctly written, otherwise returns encountered error.
    Outcome saveToFile(const Path& path) const;

    /// \brief Loads the settings from file.
    ///
    /// Previous values stored in settings are removed. The file must have a valid settings format.
    /// \param path Path to the file. The file must exist.
    /// \returns SUCCESS if the settings were correctly parsed from the file, otherwise returns encountered
    ///          error.
    Outcome loadFromFile(const Path& path);

    /// \brief If the specified file exists, loads the settings from it, otherwise creates the file and saves
    /// the current values.
    ///
    /// This is a standard behavior of configuration files used in the simulation. First time the simulation
    /// is started, the configuration files are created with default values. The files can then be modified by
    /// the user and the next time simulation is started, the parameters are parsed and used instead of the
    /// defaults.
    ///
    /// For convenience, it is possible to pass a set of overrides, which are used instead of the values
    /// loaded from the configuration file (or defaults, if the file does not exist). This can be used to
    /// specify settings as command-line parameters, for example.
    /// \param path Path to the configuration file.
    /// \param overrides Set of overrides applied on the current values or after the settings are loaded.
    /// \return True if the settings have been successfully loaded, false if the configuration file did not
    ///         exist, in which case the function attempted to create the file using current values (with
    ///         applied overrides). If loading of the file failed (invalid content, unknown parameters, ...),
    ///         an error is returned.
    ///
    /// \note Function returns a valid result (false) even if the configuration file cannot be created, for
    ///       example if the directory access is denied. This does not prohibit the simulation in any way, the
    ///       settings object is in valid state.
    Expected<bool> tryLoadFileOrSaveCurrent(const Path& path, const Settings& overrides = EMPTY_SETTINGS);

    /// \brief Iterator to the first entry of the settings storage.
    SettingsIterator<TEnum> begin() const;

    /// \brief Iterator to the one-past-end entry the settings storage.
    SettingsIterator<TEnum> end() const;

    /// \brief Returns the number of entries in the settings.
    ///
    /// This includes default entries in case the object was not constructed with EMPTY_SETTINGS tag.
    Size size() const;

    /// \\brief Returns a reference to object containing default values of all settings.
    static const Settings& getDefaults();

private:
    bool setValueByType(Entry& entry, const Value& defaultValue, const std::string& str);
};

/// \brief Iterator useful for iterating over all entries in the settings.
template <typename TEnum>
class SettingsIterator {
private:
    using ActIterator = Iterator<const typename FlatMap<TEnum, typename Settings<TEnum>::Entry>::Element>;

    ActIterator iter;

public:
    /// Constructs an iterator from iternal implementation; use Settings::begin and Settings::end.
    SettingsIterator(const ActIterator& iter);

    struct IteratorValue {
        /// ID of settings entry
        TEnum id;

        /// Variant holding the value of the entry
        typename Settings<TEnum>::Value value;
    };

    /// Dereference the iterator, yielding a pair of entry ID and its value.
    IteratorValue operator*() const;

    /// Moves to next entry.
    SettingsIterator& operator++();

    /// Equality operator between settings operators.
    bool operator==(const SettingsIterator& other) const;

    /// Unequality operator between settings operators.
    bool operator!=(const SettingsIterator& other) const;
};


enum class KernelEnum {
    /// M4 B-spline (piecewise cubic polynomial)
    CUBIC_SPLINE,

    /// M5 B-spline (piecewise 4th-order polynomial)
    FOURTH_ORDER_SPLINE,

    /// Gaussian function
    GAUSSIAN,

    /// Simple triangle (piecewise linear) kernel
    TRIANGLE,

    /// Core Triangle (CT) kernel by Read et al. (2010)
    CORE_TRIANGLE,

    /// Modification of the standard M4 B-spline kernel, used to avoid particle clustering
    THOMAS_COUCHMAN,

    /// Wendland kernel C2
    WENDLAND_C2,

    /// Wendland kernel C4
    WENDLAND_C4,

    /// Wendland kernel C6
    WENDLAND_C6,
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

    /// Modified midpoint method with constant number of substeps
    MODIFIED_MIDPOINT,

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

    /// Partitioning particles into a grid uniform in space
    UNIFORM_GRID,

    /// Selecting most suitable finder automatically
    DYNAMIC
};

enum class BoundaryEnum {
    /// Do not use any boundary conditions (= vacuum conditions)
    NONE,

    /// Highest derivatives of all particles close to the boundary are set to zero.
    FROZEN_PARTICLES,

    /// Create ghosts to keep particles inside domain.
    GHOST_PARTICLES,

    /// Creates dummy particles along the boundary.
    FIXED_PARTICLES,

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

    /// Axis-aligned ellipsoid
    ELLIPSOIDAL,

    /// Block with edge sizes given by vector
    BLOCK,

    /// Cylindrical domain aligned with z axis
    CYLINDER
};

/// List of forces to compute by the solver. This does not include numerical terms, see
/// ArtificialViscosityEnum.
enum class ForceEnum {
    /// Use force from pressure gradient in the solver.
    PRESSURE = 1 << 0,

    /// Use force from stress divergence in the model. Must be used together with PRESSURE_GRADIENT.
    /// Stress tensor is then evolved in time using Hooke's equation.
    SOLID_STRESS = 1 << 1,

    /// Stress tensor for the simulation of fluids. Must be used together with PRESSURE_GRADIENT, cannot
    /// be used together with solid stress force.
    NAVIER_STOKES = 1 << 2,

    /// Use internal friction given by the viscosity in the material.
    INTERNAL_FRICTION = 1 << 3,

    /// Use centrifugal force and Coriolis forces given by angular frequency of the coordinate frame.
    INERTIAL = 1 << 4,

    /// Use gravitational force in the model
    GRAVITY = 1 << 5,
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
    /// SPH formulation using symmetrized evaluation of derivatives.
    SYMMETRIC_SOLVER,

    /// Generic solver evaluating all derivatives asymmetrically.
    ASYMMETRIC_SOLVER,

    /// Density is obtained by direct summation over nearest SPH particles.
    SUMMATION_SOLVER,

    /// Density independent solver by Saitoh & Makino (2013).
    DENSITY_INDEPENDENT,

    /// Solver advancing internal energy using pair-wise work done by particles, by Owen (2009).
    ENERGY_CONSERVING_SOLVER,
};

enum class DiscretizationEnum {
    /// P_i / rho_i^2 + P_j / rho_j^2
    STANDARD,

    /// (P_i + P_j) / (rho_i rho_j)
    BENZ_ASPHAUG,
};

enum class YieldingEnum {
    /// Gass or material with no stress tensor
    NONE,

    /// No yielding, just elastic deformations following Hooke's law
    ELASTIC,

    /// Von Mises criterion
    VON_MISES,

    /// Drucker-Prager pressure dependent yielding stress
    DRUCKER_PRAGER
};

enum class FractureEnum {
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

enum class GravityEnum {
    /// Approximated gravity, assuming the matter is a simple homogeneous sphere.
    SPHERICAL,

    /// Brute-force summation over all particle pairs (O(N^2) complexity)
    BRUTE_FORCE,

    /// Use Barnes-Hut algorithm, approximating gravity by multipole expansion (up to octupole order)
    BARNES_HUT,
};

enum class GravityKernelEnum {
    /// Point-like particles with zero radius
    POINT_PARTICLES,

    /// Use gravity smoothing kernel corresponding to selected SPH kernel
    SPH_KERNEL,

    /// Use kernel representing gravity of solid spheres. Useful for N-body simulations where overlaps are
    /// allowed.
    SOLID_SPHERES,
};

enum class CollisionHandlerEnum {
    /// All collided particles merge, creating larger spherical particles. Particles are merged
    /// unconditionally, regardless of their relative velocity or their angular frequencies.
    PERFECT_MERGING,

    /// Collided particles bounce with some energy dissipation, specified by the coefficients of restitution.
    /// No merging, number of particles remains contant.
    ELASTIC_BOUNCE,

    /// If the relative speed of the collided particles is lower than the escape velocity, the particles are
    /// merged, otherwise the particle bounce. To ensure that the particles are always merged, set the
    /// COLLISION_MERGE_LIMIT to zero, on the other hand large values make particles more difficult to merge.
    MERGE_OR_BOUNCE,
};

enum class OverlapEnum {
    /// All overlaps are ignored
    NONE,

    /// Overlapping particles are merged
    FORCE_MERGE,

    /// Particles are shifted until no overlap happens
    REPEL,

    /// Particles are either repeled (and bounced) or merged, based on the ratio of their relative velocity to
    /// the escape velocity (similar to MERGE_OR_BOUNCE).
    REPEL_OR_MERGE,

    /// Particles are allowed to overlap and they bounce if moving towards each other.
    INTERNAL_BOUNCE,

    PASS_OR_MERGE,
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

enum class IoEnum {
    /// No input/output
    NONE,

    /// Formatted human-readable text file
    TEXT_FILE,

    /// Extension of text file, additionally executing given gnuplot script, generating a plot from every
    /// dump.
    GNUPLOT_OUTPUT,

    /// Full binary output file. This data dump is lossless and can be use to restart run from saved snapshot.
    /// Stores values, all derivatives and materials of the storage.
    BINARY_FILE,

    /// Compressed binary output file, containing only few selected quantities. This is the most convenient
    /// format for storing full simulation in high resolution in time.
    COMPRESSED_FILE,

    /// Pkdgrav input file.
    PKDGRAV_INPUT,
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
    /// User-specified name of the run, used in some output files
    RUN_NAME,

    /// User-specified comment
    RUN_COMMENT,

    /// Name of the person running the simulation
    RUN_AUTHOR,

    /// E-mail of the person running the simulation.
    RUN_EMAIL,

    /// Specifies the type of the simulation. Does not have to be specified to run the simulation; this
    /// information is saved in output files and taken into account by visualization tools, for example.
    RUN_TYPE,

    /// Selected format of the output file, see OutputEnum
    RUN_OUTPUT_TYPE,

    /// Time interval of dumping data to disk.
    RUN_OUTPUT_INTERVAL,

    /// Index of the first generated output file. Might not be zero if the simulation is resumed.
    RUN_OUTPUT_FIRST_INDEX,

    /// File name of the output file (including extension), where %d is a placeholder for output number.
    RUN_OUTPUT_NAME,

    /// Path where all output files (dumps, logs, ...) will be written
    RUN_OUTPUT_PATH,

    /// List of quantities to write to text output. Binary output always stores all quantitites.
    RUN_OUTPUT_QUANTITIES,

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

    /// Time period (in run time) of running diagnostics of the run. 0 means the diagnostics are run every
    /// time step.
    RUN_DIAGNOSTICS_INTERVAL,

    /// Index of SPH Kernel, see KernelEnum
    SPH_KERNEL,

    /// Structure for searching nearest neighbours of particles
    SPH_FINDER,

    /// Used by DynamicFinder. Maximum relative distance between center of mass and geometric center of the
    /// bounding box for which VoxelFinder is used. For larger offsets of center of mass, K-d tree is used
    /// instead.
    SPH_FINDER_COMPACT_THRESHOLD,

    /// Specifies a discretization of SPH equations; see \ref DiscretizationEnum.
    SPH_DISCRETIZATION,

    /// If true, the kernel gradient for evaluation of strain rate will be corrected for each particle by an
    /// inversion of an SPH-discretized identity matrix. This generally improves stability of the run and
    /// conservation of total angular momentum, but comes at the cost of higher memory consumption and slower
    /// evaluation of SPH derivatives.
    SPH_STRAIN_RATE_CORRECTION_TENSOR,

    /// If true, derivatives with flag DerivativeFlag::SUM_ONLY_UNDAMAGED will evaluate only undamaged
    /// particles belonging to the same body. Otherwise, all particle are evaluated, regardless of derivative
    /// flags.
    SPH_SUM_ONLY_UNDAMAGED,

    /// If true, the density derivative is computed from undamaged particles of the same body. Deformations of
    /// different bodies (even though they are in contact with the evaluated particle) or damaged particles
    /// have no effect on the density. Should be false, unless some problems in the simulation appear
    /// (instabilities, rapid growth of total energy, etc.).
    SPH_CONTINUITY_USING_UNDAMAGED,

    /// Add equations evolving particle angular velocity
    SPH_PARTICLE_ROTATION,

    /// Evolve particle phase angle
    SPH_PHASE_ANGLE,

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

    /// Type of used artificial viscosity.
    SPH_AV_TYPE,

    /// Whether to use balsara switch for computing artificial viscosity dissipation. If no artificial
    /// viscosity is used, the value has no effect.
    SPH_AV_USE_BALSARA,

    /// If true, Balsara factors will be saved as quantity AV_BALSARA. Mainly for debugging purposes.
    SPH_AV_BALSARA_STORE,

    /// Epsilon-factor of XSPH correction (Monaghan, 1992). Value 0 turns off the correction, epsilon
    /// shouldn't be larger than 1.
    XSPH_EPSILON,

    /// Delta-coefficient of the delta-SPH modification, see Marrone et al. 2011
    SPH_DENSITY_DIFFUSION_DELTA,

    /// Alpha-coefficient of the delta-SPH modification.
    SPH_VELOCITY_DIFFUSION_ALPHA,

    /// Whether to use artificial stress.
    SPH_AV_USE_STRESS,

    /// Weighting function exponent n in artificial stress term
    SPH_AV_STRESS_EXPONENT,

    /// Multiplicative factor of the artificial stress term (= strength of the viscosity)
    SPH_AV_STRESS_FACTOR,

    /// Damping coefficient of particle velocities in stabilization phase. Higher values damp the
    /// oscillation/instabilities faster, but may converge to incorrect (unstable) configuration. Lower values
    /// lead to (slower) convergence to correct (stable) configuration, but it may cause body dissintegration
    /// if the initial conditions are to far from the stable solution.
    SPH_STABILIZATION_DAMPING,

    /// If true, all particles have also a moment of inertia, representing a non-homogeneous mass
    /// distribution. Otherwise, particles are spherical with inertia tensor I = 2/5 mr^2
    NBODY_INERTIA_TENSOR,

    /// \todo
    NBODY_MAX_ROTATION_ANGLE,

    /// Algorithm to compute gravitational acceleration
    GRAVITY_SOLVER,

    /// Opening angle Theta for multipole approximation of gravity
    GRAVITY_OPENING_ANGLE,

    /// Maximum number of particles in a leaf node.
    GRAVITY_LEAF_SIZE,

    /// Order of multipole expansion
    GRAVITY_MULTIPOLE_ORDER,

    /// Gravity smoothing kernel
    GRAVITY_KERNEL,

    /// Period of gravity evaluation. If zero, gravity is computed every time step, for any positive value,
    /// gravitational acceleration is cached for each particle and used each time step until next
    /// recomputation.
    GRAVITY_RECOMPUTATION_PERIOD,

    /// Specifies how the collisions of particles should be handler; see CollisionHandlerEnum.
    COLLISION_HANDLER,

    /// Specifies how particle overlaps should be handled.
    COLLISION_OVERLAP,

    /// Coefficient of restitution for normal component (alongside particle direction vector) of velocity.
    /// Applicable only for bounce collisions.
    COLLISION_RESTITUTION_NORMAL,

    /// Coefficient of restitution for tangent component (perpendicular to particle direction vector) of
    /// velocity. Applicable only for bounce collisions.
    COLLISION_RESTITUTION_TANGENT,

    /// Relative particle overlap (0 for particles in contact, 1 for particles lying on top of each other) for
    /// which the collision is handled as overlap. Used to avoid very small (<EPS) overlaps not being handled
    /// as collision due to numerical imprecisions.
    COLLISION_ALLOWED_OVERLAP,

    /// Multiplier of the relative velocity, used when determining whether to merge the collided particles or
    /// reject the collision. If zero, particles are always merged. Larger values than 1 can be used to merge
    /// only very slowly moving particles.
    COLLISION_BOUNCE_MERGE_LIMIT,

    /// Parameter analogous to COLLISION_BOUNCE_MERGE_LIMIT, but used for the rotation of the merger.
    /// Particles can only be merged if the angular frequency multiplied by this parameter is lower than the
    /// breakup frequency. If zero, particles are always merged, values larger than 1 can be used to avoid
    /// fast rotators in the simulation.
    COLLISION_ROTATION_MERGE_LIMIT,

    /// Selected solver for computing derivatives of physical variables.
    SOLVER_TYPE,

    /// List of forces to compute by the solver.
    SOLVER_FORCES,

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
    TIMESTEPPING_COURANT_NUMBER,

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

    /// Power of the generalized mean, used to compute the final timestep from timesteps of individual
    /// particles. Negative infinity means the minimal timestep is used. This value will also set statistics
    /// of the restricting particle, namely the particle index and the quantity value and corresponding
    /// derivative of the particle; these statistics are not saved for other powers.
    TIMESTEPPING_MEAN_POWER,

    /// Maximum relative increase of a timestep in two subsequent timesteps. Used to 'smooth' the timesteps,
    /// as it prevents very fast increase of the time step, especially if the initial time step is very low.
    /// Used only by \ref MultiCriterion.
    TIMESTEPPING_MAX_INCREASE,

    /// Number of sub-steps in the modified midpoint method.
    TIMESTEPPING_MIDPOINT_COUNT,

    /// Required relative accuracy of the Bulirsch-Stoer integrator.
    TIMESTEPPING_BS_ACCURACY,

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
    /// No equation of state
    NONE,

    /// Equation of state for ideal gas
    IDEAL_GAS,

    /// Tait equation of state for simulations of liquids
    TAIT,

    /// Mie-Gruneisen equation of state
    MIE_GRUNEISEN,

    /// Tillotson (1962) equation of state
    TILLOTSON,

    /// Murnaghan equation of state
    MURNAGHAN,

    /// ANEOS given by look-up table
    ANEOS
};

/// \brief Settings of a single body / gas phase / ...
///
/// Combines material parameters and numerical parameters of the SPH method specific for one body. Values of
/// parameters CANNOT be changed to preserve backward compatibility of serializer. New IDs can be created as
/// needed, parameters no longer needed can be removed.
enum class BodySettingsId {
    /// Equation of state for this material, see EosEnum for options.
    EOS = 0,

    /// Initial distribution of SPH particles within the domain, see DistributionEnum for options.
    INITIAL_DISTRIBUTION = 1,

    /// If true, generated particles will be moved so that their center of mass corresponds to the center of
    /// selected domain. Note that this will potentially move some particles outside of the domain, which can
    /// clash with boundary conditions.
    CENTER_PARTICLES = 2,

    /// If true, particles are sorted using Morton code, preserving locality in memory.
    PARTICLE_SORTING = 3,

    /// Turns on 'SPH5 compatibility' mode when generating particle positions. This allows 1-1 comparison of
    /// generated arrays, but results in too many generated particles (by about factor 1.4). The option also
    /// implies CENTER_PARTICLES.
    DISTRIBUTE_MODE_SPH5 = 4,

    /// Strength parameter of the Diehl's distribution.
    DIELH_STRENGTH = 5,

    /// Maximum allowed difference between the expected number of particles and the actual number of generated
    /// particles. Higher value speed up the generation of particle positions.
    DIEHL_MAX_DIFFERENCE = 6,

    /// Density at zero pressure
    DENSITY = 7,

    /// Allowed range of density. Densities of all particles all clamped to fit in the range.
    DENSITY_RANGE = 8,

    /// Estimated minimal value of density. This value is NOT used to clamp densities, but for
    /// determining error of timestepping.
    DENSITY_MIN = 9,

    /// Initial specific internal energy
    ENERGY = 10,

    /// Allowed range of specific internal energy.
    ENERGY_RANGE = 11,

    /// Estimated minimal value of energy used to determine timestepping error.
    ENERGY_MIN = 12,

    /// Initial values of the deviatoric stress tensor
    STRESS_TENSOR = 13,

    /// Estimated minial value of stress tensor components used to determined timestepping error.
    STRESS_TENSOR_MIN = 14,

    /// Initial damage of the body.
    DAMAGE = 15,

    /// Allowed range of damage.
    DAMAGE_RANGE = 16,

    /// Estimate minimal value of damage used to determine timestepping error.
    DAMAGE_MIN = 17,

    /// Adiabatic index used by some equations of state (such as ideal gas)
    ADIABATIC_INDEX = 18,

    /// Exponent of density, representing a compressibility of a fluid. Used in Tait equation of state.
    TAIT_GAMMA = 19,

    /// Sound speed used in Tait equation of state
    TAIT_SOUND_SPEED = 20,

    /// Bulk modulus of the material
    BULK_MODULUS = 21,

    /// Coefficient B of the nonlinear compressive term in Tillotson equation
    TILLOTSON_NONLINEAR_B = 22,

    /// "Small a" coefficient in Tillotson equation
    TILLOTSON_SMALL_A = 23,

    /// "Small b" coefficient in Tillotson equation
    TILLOTSON_SMALL_B = 24,

    /// Alpha coefficient in expanded phase of Tillotson equation
    TILLOTSON_ALPHA = 25,

    /// Beta coefficient in expanded phase of Tillotson equation
    TILLOTSON_BETA = 26,

    /// Specific sublimation energy
    TILLOTSON_SUBLIMATION = 27,

    /// Specific energy of incipient vaporization
    TILLOTSON_ENERGY_IV = 28,

    /// Specific energy of complete vaporization
    TILLOTSON_ENERGY_CV = 29,

    /// Gruneisen's gamma paraemter used in Mie-Gruneisen equation of state
    GRUNEISEN_GAMMA = 30,

    /// Used in Mie-Gruneisen equations of state
    BULK_SOUND_SPEED = 31,

    /// Linear Hugoniot slope coefficient used in Mie-Gruneisen equation of state
    HUGONIOT_SLOPE = 32,

    RHEOLOGY_YIELDING = 33,

    RHEOLOGY_DAMAGE = 34,

    /// Shear modulus mu (a.k.a Lame's second parameter) of the material
    SHEAR_MODULUS = 35,

    YOUNG_MODULUS = 36,

    /// Elastic modulus lambda (a.k.a Lame's first parameter) of the material
    ELASTIC_MODULUS = 37,

    /// Elasticity limit of the von Mises yielding criterion
    ELASTICITY_LIMIT = 38,

    MELT_ENERGY = 39,

    /// Cohesion, yield strength at zero pressure
    COHESION = 40,

    /// Coefficient of friction for undamaged material
    INTERNAL_FRICTION = 41,

    /// Coefficient of friction for fully damaged material
    DRY_FRICTION = 42,

    /// \todo
    BRITTLE_DUCTILE_TRANSITION_PRESSURE = 43,

    /// \todo
    BRITTLE_PLASTIC_TRANSITION_PRESSURE = 44,

    /// \todo
    MOHR_COULOMB_STRESS = 45,

    /// \todo
    FRICTION_ANGLE = 46,

    /// Speed of crack growth, in units of local sound speed.
    RAYLEIGH_SOUND_SPEED = 47,

    WEIBULL_COEFFICIENT = 48,

    WEIBULL_EXPONENT = 49,

    BULK_VISCOSITY = 50,

    SHEAR_VISCOSITY = 51,

    DAMPING_COEFFICIENT = 52,

    DIFFUSIVITY = 53,

    /// Coefficient of surface tension
    SURFACE_TENSION = 54,

    /// Number of SPH particles in the body
    PARTICLE_COUNT = 55,

    /// Minimal number of particles per one body. Used when creating 'sub-bodies' withing one 'parent' body,
    /// for example when creating rubble-pile asteroids, ice blocks inside an asteroid, etc. Parameter has no
    /// effect for creation of a single monolithic body; the number of particles from PARTICLE_COUNT is used
    /// in any case.
    MIN_PARTICLE_COUNT = 56,

    /// Initial alpha coefficient of the artificial viscosity. This is only used if the coefficient is
    /// different for each particle. For constant coefficient shared for all particles, use value from global
    /// settings.
    AV_ALPHA = 57,

    /// Lower and upper bound of the alpha coefficient, used only for time-dependent artificial viscosity.
    AV_ALPHA_RANGE = 58,

    /// Center point of the body. Currently used only by \ref StabilizationSolver.
    BODY_CENTER = 61,

    /// Velocity of the body. `Currently used only by \ref StabilizationSolver.
    BODY_VELOCITY = 62,

    /// Angular frequency of the body with a respect to position given by BODY_CENTER. Currently used only by
    /// \ref StabilizationSolver.
    BODY_SPIN_RATE = 63,

    /// Bulk (macro)porosity of the body
    BULK_POROSITY = 64,

    /// Heat capacity at constant pressure,
    HEAT_CAPACITY = 65,

    /// Arbitrary string identifying this material
    IDENTIFIER = 99,
};

using RunSettings = Settings<RunSettingsId>;
template <>
AutoPtr<RunSettings> RunSettings::instance;

using BodySettings = Settings<BodySettingsId>;
template <>
AutoPtr<BodySettings> BodySettings::instance;

NAMESPACE_SPH_END
