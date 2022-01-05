#pragma once

/// \file Settings.h
/// \brief Generic storage and input/output routines of settings.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/Exceptions.h"
#include "objects/geometry/TracelessTensor.h"
#include "objects/utility/EnumMap.h"
#include "objects/wrappers/ClonePtr.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/Outcome.h"
#include "objects/wrappers/Variant.h"
#include <typeinfo>

NAMESPACE_SPH_BEGIN

class Path;

template <typename TEnum>
class Settings;

template <typename TEnum>
class SettingsIterator;

/// Tag for initialization of empty settings object.
struct EmptySettingsTag {};

const EmptySettingsTag EMPTY_SETTINGS;

/// \brief Wrapper of an enum.
///
/// Used to store an enum in settings while keeping the type safety.
struct EnumWrapper {
    int value = 0;

    EnumIndex index = NOTHING;

    EnumWrapper() = default;

    template <typename TEnum>
    explicit EnumWrapper(TEnum e)
        : value(int(e))
        , index(typeid(TEnum)) {
        static_assert(std::is_enum<TEnum>::value, "Can be used only for enums");
    }

    EnumWrapper(const int value, const EnumIndex& index)
        : value(value)
        , index(index) {}

    explicit operator int() const {
        return value;
    }

    template <typename T, typename = std::enable_if_t<std::is_enum<T>::value>>
    explicit operator T() const {
        SPH_ASSERT(std::type_index(typeid(T)) == index);
        return T(value);
    }

    bool operator==(const EnumWrapper& other) const {
        return value == other.value && index == other.index;
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& ofs, const EnumWrapper& e) {
        ofs << e.value << " (" << (e.index ? e.index->hash_code() : 0) << ")";
        return ofs;
    }
};

/// \brief Exception thrown on invalid access to entries of a \ref Settings object.
class InvalidSettingsAccess : public Exception {
public:
    template <typename TEnum>
    explicit InvalidSettingsAccess(const TEnum key)
        : Exception("Error accessing parameter '" +
                    Settings<TEnum>::getEntryName(key).valueOr("unknown parameter") + "'") {
        static_assert(std::is_enum<TEnum>::value, "InvalidSettingsAccess can only be used with enums");
    }
};

template <typename TEnum>
INLINE void checkSettingsAccess(const bool result, const TEnum key) {
    if (!result) {
        throw InvalidSettingsAccess(key);
    }
}

/// \brief Generic object containing various settings and parameters of the run.
///
/// Settings is a storage containing pairs key-value objects, where key is one of predefined enums. The value
/// can have multiple types within the same \ref Settings object. Currently following types can be stored:
/// bool, int, enums, float (or double), String, \ref Interval, \ref Vector, \ref SymmetricTensor, \ref
/// TracelessTensor.
///
/// The template cannot be used directly as it is missing default values of parameters; instead
/// specializations for specific enums should be used. The code defines two base specializations:
///     - \ref BodySettings (specialization with enum \ref BodySettingsId)
///     - \ref RunSettings (specialization with enum \ref RunSettingsId)
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
    using Value =
        Variant<bool, int, Float, Interval, String, Vector, SymmetricTensor, TracelessTensor, EnumWrapper>;

    struct Entry {
        /// Index of the property
        TEnum id;

        /// Unique text identifier of the property
        String name;

        /// Current value
        Value value;

        /// Description of the property. Can be empty.
        String desc;

        Entry() = default;

        Entry(TEnum id, const String& name, const Value& value, const String& desc = "")
            : id(id)
            , name(name)
            , value(value)
            , desc(desc) {}

        template <typename T, typename = std::enable_if_t<std::is_enum<T>::value>>
        Entry(TEnum id, const String& name, const T& value, const String& desc = "")
            : id(id)
            , name(name)
            , value(EnumWrapper(value))
            , desc(desc) {}

        template <typename T>
        Entry(TEnum id, const String& name, Flags<T> flags, const String& desc = "")
            : id(id)
            , name(name)
            , value(EnumWrapper(T(flags.value())))
            , desc(desc) {}

        template <typename T>
        INLINE bool hasType(std::enable_if_t<!std::is_enum<T>::value, int> = 0) const {
            return value.has<T>();
        }

        template <typename T>
        INLINE bool hasType(std::enable_if_t<std::is_enum<T>::value, int> = 0) const {
            return value.has<EnumWrapper>() && value.get<EnumWrapper>().index == std::type_index(typeid(T));
        }
    };

    FlatMap<TEnum, Entry> entries;

public:
    /// \brief Initialize settings by settings all value to their defaults.
    Settings();

    /// Constructs settings from list of key-value pairs.
    Settings(std::initializer_list<Entry> list);

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
    /// \throw InvalidSettingsAccess if settings value of different type than the one currently stored.
    template <typename TValue>
    Settings& set(const TEnum idx,
        TValue&& value,
        std::enable_if_t<!std::is_enum<std::decay_t<TValue>>::value, int> = 0) {
        Optional<Entry&> entry = entries.tryGet(idx);
        if (!entry) {
            Entry& newEntry = entries.insert(idx, Entry{});
            newEntry.value = std::forward<TValue>(value);
        } else {
            checkSettingsAccess(entry->template hasType<std::decay_t<TValue>>(), idx);
            entry->value = std::forward<TValue>(value);
        }
        return *this;
    }

    /// \copydoc set
    template <typename TValue>
    Settings& set(const TEnum idx,
        TValue&& value,
        std::enable_if_t<std::is_enum<std::decay_t<TValue>>::value, int> = 0) {
        Optional<Entry&> entry = entries.tryGet(idx);
        if (!entry) {
            Entry& newEntry = entries.insert(idx, Entry{});
            newEntry.value = EnumWrapper(value);
        } else {
            checkSettingsAccess(entry->template hasType<std::decay_t<TValue>>(), idx);
            entry->value = EnumWrapper(value);
        }
        return *this;
    }

    /// \brief Saves flags into the settings.
    ///
    /// Flags are not considered as a different type, they are internally saved as enum wrapper.
    /// \returns Reference to the settings object.
    /// \throw InvalidSettingsAccess if settings value of different type than the one currently stored.
    template <typename TValue, typename = std::enable_if_t<std::is_enum<TValue>::value>>
    Settings& set(const TEnum idx, const Flags<TValue> flags) {
        return this->set(idx, EnumWrapper(TValue(flags.value())));
    }

    /// \brief Clear flags of given parameter in settings.
    ///
    /// This function can be used only if the value is already stored in the settings.
    /// \returns Reference to the settings object.
    /// \throw InvalidSettingsAccess if the value is not stored or has a different type.
    Settings& set(const TEnum idx, EmptyFlags) {
        Optional<Entry&> entry = entries.tryGet(idx);
        checkSettingsAccess(entry && entry->value.template has<EnumWrapper>(), idx);
        entry->value.template get<EnumWrapper>().value = 0;
        return *this;
    }

    /// \brief Special setter for value of type EnumWrapper.
    ///
    /// While usually the enum can be stored directly, this overload is useful for deserialization, etc.
    /// \returns Reference to the settings object.
    /// \throw InvalidSettingsAccess if settings value of different type than the one currently stored.
    Settings& set(const TEnum idx, const EnumWrapper ew) {
        Optional<Entry&> entry = entries.tryGet(idx);
        if (entry) {
            Optional<EnumWrapper> current = entry->value.template tryGet<EnumWrapper>();
            checkSettingsAccess(current && current->index == ew.index, idx);
        }
        this->set(idx, ew, 0); // zero needed to call the other overload
        return *this;
    }

    /// \brief Adds entries from different \ref Settings object into this one, overriding current entries.
    ///
    /// Entries not stored in the given settings are kept unchanged.
    /// \throw InvalidSettingsAccess if the settings share entries that differ in type.
    void addEntries(const Settings& settings) {
        for (const typename SettingsIterator<TEnum>::IteratorValue& iv : settings) {
            Optional<Entry&> entry = entries.tryGet(iv.id);
            if (!entry) {
                Entry& newEntry = entries.insert(iv.id, getDefaults().entries[iv.id]);
                newEntry.value = iv.value;
            } else {
                checkSettingsAccess(entry->value.getTypeIdx() == iv.value.getTypeIdx(), iv.id);
                entry->value = iv.value;
            }
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
    /// Value must be stored in settings and must have corresponding type.
    /// \tparam TValue Type of the value we wish to return. This type must match the type of the saved
    ///                quantity.
    /// \param idx Key of the value.
    /// \returns Value correponsing to given key.
    /// \throw InvalidSettingsAccess if value is not stored in the settings or has a different type.
    template <typename TValue>
    TValue get(const TEnum idx, std::enable_if_t<!std::is_enum<std::decay_t<TValue>>::value, int> = 0) const {
        Optional<const Entry&> entry = entries.tryGet(idx);
        checkSettingsAccess(entry && entry->value.template has<TValue>(), idx);

        return entry->value.template get<TValue>();
    }

    /// \copydoc get
    template <typename TValue>
    TValue get(const TEnum idx, std::enable_if_t<std::is_enum<std::decay_t<TValue>>::value, int> = 0) const {
        Optional<const Entry&> entry = entries.tryGet(idx);
        checkSettingsAccess(entry && entry->value.template has<EnumWrapper>(), idx);

        const EnumWrapper wrapper = entry->value.template get<EnumWrapper>();
        checkSettingsAccess(wrapper.index == std::type_index(typeid(TValue)), idx);
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

    /// \brief Returns the human-readable name of the entry with given index.
    ///
    /// If the index does not correspond to any parameter, returns string NOTHING.
    static Optional<String> getEntryName(const TEnum idx) {
        const Settings& settings = getDefaults();
        Optional<const Entry&> entry = settings.entries.tryGet(idx);
        if (entry) {
            return entry->name;
        } else {
            // idx might be invalid if loaded from older config file
            return NOTHING;
        }
    }

    /// \brief Returns the type of the entry with given index.
    ///
    /// If the index does not correspond to any parameter, returns NOTHING.
    static Optional<int> getEntryType(const TEnum idx) {
        const Settings& settings = getDefaults();
        Optional<const Entry&> entry = settings.entries.tryGet(idx);
        if (entry) {
            return entry->value.getTypeIdx();
        } else {
            return NOTHING;
        }
    }

    /// \brief Returns the string name for given type index.
    ///
    /// \throw Exception for unknown type index
    static String typeToString(const int type);

    /// \brief Returns a description of the entry with given index.
    ///
    /// If the index does not correspond to any parameter, returns string NOTHING.
    static Optional<String> getEntryDesc(const TEnum idx) {
        const Settings& settings = getDefaults();
        Optional<const Entry&> entry = settings.entries.tryGet(idx);
        if (entry) {
            return entry->desc;
        } else {
            // idx might be invalid if loaded from older config file
            return NOTHING;
        }
    }

    /// \brief Returns an ID for given entry name.
    ///
    /// This is the inverse of function \ref getEntryName.
    static Optional<TEnum> getEntryId(const String& name) {
        const Settings& settings = getDefaults();
        for (const auto& p : settings.entries) {
            if (p.value().name == name) {
                return p.key();
            }
        }
        return NOTHING;
    }

    /// \brief Checks if the given entry is stored in the settings.
    bool has(const TEnum idx) const {
        return entries.contains(idx);
    }

    /// \brief Checks if the given entry has specified type.
    ///
    /// \throw InvalidSettingsAccess if the entry is not in settings.
    template <typename TValue>
    bool hasType(const TEnum idx) const {
        Optional<const Entry&> entry = entries.tryGet(idx);
        checkSettingsAccess(bool(entry), idx);
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
    /// \returns True if the settings have been successfully loaded, false if the configuration file did not
    ///         exist, in which case the function attempted to create the file using current values (with
    ///         applied overrides).
    /// \throw Exception if loading of the file failed (invalid content, unknown parameters, ...).
    ///
    /// \note Function returns a valid result (false) even if the configuration file cannot be created, for
    ///       example if the directory access is denied. This does not prohibit the simulation in any way, the
    ///       settings object is in valid state.
    bool tryLoadFileOrSaveCurrent(const Path& path, const Settings& overrides = EMPTY_SETTINGS);

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
    bool setValueByType(Entry& entry, const Value& defaultValue, const String& str);
};

/// \brief Returns the default settings.
///
/// Needs to be specialized for each TEnum.
template <typename TEnum>
const Settings<TEnum>& getDefaultSettings();

/// \brief Iterator useful for iterating over all entries in the settings.
template <typename TEnum>
class SettingsIterator {
private:
    using ActIterator = Iterator<const typename FlatMap<TEnum, typename Settings<TEnum>::Entry>::Element>;

    ActIterator iter;

public:
    /// Constructs an iterator from iternal implementation; use Settings::begin and Settings::end.
    SettingsIterator(const ActIterator& iter, Badge<Settings<TEnum>>);

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
    /// Time step determined using CFL condition
    COURANT = 1 << 1,

    /// Time step computed by limiting value-to-derivative ratio of quantiites.
    DERIVATIVES = 1 << 2,

    /// Time step computed from ratio of acceleration and smoothing length.
    ACCELERATION = 1 << 3,

    /// Time step computed from velocity divergence
    DIVERGENCE = 1 << 4,

    /// Value for using all criteria.
    ALL = COURANT | DERIVATIVES | ACCELERATION | DIVERGENCE,
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

    /// Using hash map
    HASH_MAP,
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

    /// Particles are duplicated along the z=0 plane
    SYMMETRIC,

    /// Removes particles outside the domain
    KILL_ESCAPERS,

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
    CYLINDER,

    /// Gaussian random sphere
    GAUSSIAN_SPHERE,

    /// Half-space z>0
    HALF_SPACE,
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
    SELF_GRAVITY = 1 << 5,

    /// Surface force proportional to surface curvature
    SURFACE_TENSION = 1 << 6,
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

    /// Special solver used to simulate deformations of perfectly elastic bodies
    ELASTIC_DEFORMATION_SOLVER,

    /// Density independent solver by Saitoh & Makino (2013).
    DENSITY_INDEPENDENT,

    /// Solver advancing internal energy using pair-wise work done by particles, by Owen (2009).
    ENERGY_CONSERVING_SOLVER,

    /// Simple solver with pressure gradient only, mainly used for supporting purposes (benchmarking,
    /// teaching, etc.)
    SIMPLE_SOLVER,
};

enum class ContinuityEnum {
    /// Normal continuity equation, using velocity divergence computed from all neighbors.
    STANDARD,

    /// Computes the velocity divergence using only undamaged neighbors. For fully damaged particle, the
    /// standard continuity equation is used instead.
    SUM_ONLY_UNDAMAGED,
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
    DRUCKER_PRAGER,

    /// No stress tensor, only the pressure is limited to positive values.
    DUST,
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
    /// Smoothing length is evolved using continuity equation
    CONTINUITY_EQUATION = 1 << 1,

    /// Number of neighbors is kept fixed by adding additional derivatives of smoothing length, scaled by
    /// local sound speed
    SOUND_SPEED_ENFORCING = 1 << 2
};

enum class SignalSpeedEnum {
    /// Signal speed given by the absolute value of pressure difference, as in Price (2008)
    PRESSURE_DIFFERENCE,

    /// Signal speed given by relative velocity projected to the positive vector, as in Valdarnini (2018),
    VELOCITY_DIFFERENCE,
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
    /// No collision handling
    NONE,

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
    NONE = 0,

    /// Formatted human-readable text file
    TEXT_FILE = 1,

    /// Full binary output file. This data dump is lossless and can be use to restart run from saved snapshot.
    /// Stores values, all derivatives and materials of the storage.
    BINARY_FILE = 3,

    /// Compressed binary output file, containing only few selected quantities. This is the most convenient
    /// format for storing full simulation in high resolution in time.
    DATA_FILE = 4,

    /// File format used by Visualization Toolkit (VTK). Useful to view the results in Paraview and other
    /// visualization tools.
    VTK_FILE = 5,

    /// File format for storing scientific data. Currently tailored for files generated by the code
    /// miluphcuda. Requires to build code with libhdf5.
    HDF5_FILE = 6,

    /// Export from Minor Planet Center Orbit Database
    MPCORP_FILE = 7,

    /// Pkdgrav input file.
    PKDGRAV_INPUT = 8,
};

/// \brief Returns the file extension associated with given IO type.
///
/// Result NOTHING indicates there is no particular extension associated with the IO type.
Optional<String> getIoExtension(const IoEnum type);

/// \brief Returns the file type from file extension.
///
/// Result NOTHING indicates there is no file type associated with the extension.
Optional<IoEnum> getIoEnum(const String& ext);

/// \brief Returns a short description of the file format.
String getIoDescription(const IoEnum type);

enum class IoCapability {
    /// The format can be used as file input
    INPUT = 1 << 1,

    /// The format can be used as file output
    OUTPUT = 1 << 2,
};

/// \brief Returns the capabilities of given file format.
Flags<IoCapability> getIoCapabilities(const IoEnum type);


enum class OutputSpacing {
    /// Constant time between consecutive output times
    LINEAR,

    /// Constant ratio between consecutive output times
    LOGARITHMIC,

    /// User-defined list of output times
    CUSTOM,
};

enum class RngEnum {
    /// Mersenne Twister PRNG from Standard library
    UNIFORM,

    /// Halton QRNG
    HALTON,

    /// Same RNG as used in SPH5, used for 1-1 comparison
    BENZ_ASPHAUG
};

enum class UvMapEnum {
    /// Planar mapping
    PLANAR,

    /// Spherical mapping
    SPHERICAL,
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

    /// Selected format of the output file, see \ref IoEnum
    RUN_OUTPUT_TYPE,

    /// Time interval of dumping data to disk.
    RUN_OUTPUT_INTERVAL,

    /// Type of output spacing in time, see enum OutputSpacing
    RUN_OUTPUT_SPACING,

    /// List of comma-separated output times, used when RUN_OUTPUT_SPACING is set to CUSTOM
    RUN_OUTPUT_CUSTOM_TIMES,

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

    /// Number specifying log verbosity. Can be between 0 and 3, going from least to most verbose.
    RUN_LOGGER_VERBOSITY,

    /// Enables verbose log of a simulation.
    RUN_VERBOSE_ENABLE,

    /// Path of a file where the verbose log is printed.
    RUN_VERBOSE_NAME,

    /// Starting time of the simulation in seconds. This is usually 0, although it can be set to a non-zero
    /// for simulations resumed from saved state.
    RUN_START_TIME,

    /// End time of the simulation in seconds. For new simulations (not resumed from saved state), this
    /// corresponds to the total duration of the simulation.
    RUN_END_TIME,

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

    /// Selected solver for computing derivatives of physical variables.
    SPH_SOLVER_TYPE,

    /// List of forces to compute by the solver.
    SPH_SOLVER_FORCES,

    /// Solution for evolutions of the smoothing length
    SPH_ADAPTIVE_SMOOTHING_LENGTH,

    /// Maximum number of iterations for self-consistent density computation of summation solver.
    SPH_SUMMATION_MAX_ITERATIONS,

    /// Target relative difference of density in successive iterations. Density computation in summation
    /// solver is ended when the density changes by less than the delta or the iteration number exceeds
    /// SOLVER_SUMMATION_MAX_ITERATIONS.
    SPH_SUMMATION_DENSITY_DELTA,

    /// If true, the SPH solver computes a hash map connecting position in space with required search radius.
    /// Otherwise, the radius is determined from the maximal smoothing length in the simulation. Used only by
    /// the AsymmetricSolver.
    SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP,

    /// Index of SPH Kernel, see KernelEnum
    SPH_KERNEL,

    /// Structure for searching nearest neighbors of particles
    SPH_FINDER,

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

    /// Specifies how the density is evolved, see \ref ContinuityEnum.
    SPH_CONTINUITY_MODE,

    /// Evolve particle phase angle
    SPH_PHASE_ANGLE,

    /// Minimum and maximum number of neighbors SPH solver tries to enforce. Note that the solver cannot
    /// guarantee the actual number of neighbors will be within the range.
    SPH_NEIGHBOR_RANGE,

    /// Strength of enforcing neighbor number. Higher value makes enforcing more strict (number of neighbors
    /// gets into required range faster), but also makes code less stable. Can be a negative number, -INFTY
    /// technically disables enforcing altogether.
    SPH_NEIGHBOR_ENFORCING,

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

    /// Enables the artificial thermal conductivity term.
    SPH_USE_AC,

    /// Artificial conductivity alpha coefficient
    SPH_AC_ALPHA,

    /// Artificial conductivity beta coefficient
    SPH_AC_BETA,

    /// Type of the signal speed used by artificial conductivity
    SPH_AC_SIGNAL_SPEED,

    /// Turn on the XSPH correction
    SPH_USE_XSPH,

    /// Epsilon-factor of XSPH correction (Monaghan, 1992). Value 0 turns off the correction, epsilon
    /// shouldn't be larger than 1.
    SPH_XSPH_EPSILON,

    /// Turn on the delta-SPH correction
    SPH_USE_DELTASPH,

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

    /// Damping coefficient of particle velocities, mainly intended for setting up initial conditions. Higher
    /// values damp the oscillation/instabilities faster, but may converge to incorrect (unstable)
    /// configuration. Lower values lead to (slower) convergence to correct (stable) configuration, but it may
    /// cause body dissintegration if the initial conditions are to far from the stable solution. Zero
    /// disables the damping.
    SPH_STABILIZATION_DAMPING,

    /// Alpha parameter of the density-independent solver.
    SPH_DI_ALPHA,

    /// Enables or disables scripted term.
    SPH_SCRIPT_ENABLE,

    /// Path of an arbitrary ChaiScript script executed each timestep.
    SPH_SCRIPT_FILE,

    /// Period or time point to execute the script. Zero means the time step is executed immediately or every
    /// time step, depending on the value of SPH_SCRIPT_ONESHOT.
    SPH_SCRIPT_PERIOD,

    /// Whether to execute the script only once or periodically.
    SPH_SCRIPT_ONESHOT,

    /// If true, all particles have also a moment of inertia, representing a non-homogeneous mass
    /// distribution. Otherwise, particles are spherical with inertia tensor I = 2/5 mr^2
    NBODY_INERTIA_TENSOR,

    /// \todo
    NBODY_MAX_ROTATION_ANGLE,

    /// If true, colliding particles form aggregates, which then move and rotate as rigid bodies. There are no
    /// collisions between particles belonging to the same aggregate, only collisions of different aggregates
    /// need to be handled. Note that enabling aggregates overrides handlers of collisions and overlaps.
    NBODY_AGGREGATES_ENABLE,

    /// Specifies the initial aggregates used in the simulation. See \ref AggregateEnum.
    NBODY_AGGREGATES_SOURCE,

    /// Algorithm to compute gravitational acceleration
    GRAVITY_SOLVER,

    /// Opening angle Theta for multipole approximation of gravity
    GRAVITY_OPENING_ANGLE,

    /// Order of multipole expansion
    GRAVITY_MULTIPOLE_ORDER,

    /// Gravity smoothing kernel
    GRAVITY_KERNEL,

    /// Gravitational constant. To be generalized.
    GRAVITY_CONSTANT,

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

    /// Magnitude of the repel force for the soft-body solver
    SOFT_REPEL_STRENGTH,

    /// Magnitude of the friction force for the soft-body solver
    SOFT_FRICTION_STRENGTH,

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

    /// Multiplicative factor k for the derivative criterion; dt = k * v / dv
    TIMESTEPPING_DERIVATIVE_FACTOR,

    /// Multiplicative factor for the divergence criterion
    TIMESTEPPING_DIVERGENCE_FACTOR,

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

    /// Stores per-particle time steps
    SAVE_PARTICLE_TIMESTEPS,

    /// Global rotation of the coordinate system around axis (0, 0, 1) passing through origin. If non-zero,
    /// causes non-intertial acceleration.
    FRAME_ANGULAR_FREQUENCY,

    FRAME_CONSTANT_ACCELERATION,

    FRAME_TIDES_MASS,

    FRAME_TIDES_POSITION,

    /// Maximum number of particles in a leaf node.
    FINDER_LEAF_SIZE,

    /// Maximal in a a tree depth to be processed in parallel. While a larger value implies better
    /// distribution of work between threads, it also comes up with performance penalty due to scheduling
    /// overhead.
    FINDER_MAX_PARALLEL_DEPTH,

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

    /// If true, the mapping coordinates will be computed and saved for all bodies in the simulation.
    GENERATE_UVWS,

    /// Type of the UV mapping
    UVW_MAPPING,
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

    /// Stratified distribution to reduce clustering
    STRATIFIED,

    /// Parametrized spiraling scheme by Saff & Kuijlaars (1997)
    PARAMETRIZED_SPIRALING,

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

    /// Simplified version of the Tillotson equation of state
    SIMPLIFIED_TILLOTSON,

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
    DIEHL_STRENGTH = 5,

    /// Maximum allowed difference between the expected number of particles and the actual number of generated
    /// particles. Higher value speed up the generation of particle positions.
    DIEHL_MAX_DIFFERENCE = 6,

    /// Number of iterations of particle repelling.
    DIEHL_ITERATION_COUNT = 71,

    /// Eta-factor between smoothing length and particle concentration (h = eta * n^(-1/d) )
    SMOOTHING_LENGTH_ETA = 72,

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

    /// Model of stress reducing used within the rheological model
    RHEOLOGY_YIELDING = 33,

    /// Model of fragmentation used within the rheological model
    RHEOLOGY_DAMAGE = 34,

    /// Shear modulus mu (a.k.a Lame's second parameter) of the material
    SHEAR_MODULUS = 35,

    /// Young modulus of the material
    YOUNG_MODULUS = 36,

    /// Elastic modulus lambda (a.k.a Lame's first parameter) of the material
    ELASTIC_MODULUS = 37,

    /// Elasticity limit of the von Mises yielding criterion
    ELASTICITY_LIMIT = 38,

    /// Melting energy, used for temperature-dependence of the stress tensor
    MELT_ENERGY = 39,

    /// Cohesion, yield strength at zero pressure
    COHESION = 40,

    /// Coefficient of friction for undamaged material
    INTERNAL_FRICTION = 41,

    /// Coefficient of friction for fully damaged material
    DRY_FRICTION = 42,

    /// Whether to use the acoustic fludization model.
    USE_ACOUSTIC_FLUDIZATION = 43,

    /// Characteristic decay time of acoustic oscillations in the material
    OSCILLATION_DECAY_TIME = 44,

    /// Regeneration efficiency of acoustric oscillations
    OSCILLATION_REGENERATION = 74,

    /// Effective viscosity of acoustic fludizations
    FLUIDIZATION_VISCOSITY = 45,

    /// Speed of crack growth, in units of local sound speed.
    RAYLEIGH_SOUND_SPEED = 47,

    /// Coefficient (multiplier) of the Weibull distribution of flaws
    WEIBULL_COEFFICIENT = 48,

    /// Exponent of the Weibull distribution of flaws
    WEIBULL_EXPONENT = 49,

    /// Whether to use precomputed distributions for flaw sampling. Otherwise, flaws are sampled using uniform
    /// random number generator, which may be prohibitively slow for large particle counts.
    WEIBULL_SAMPLE_DISTRIBUTIONS = 66,

    /// Initial value of the material distention, used in the P-alpha model.
    DISTENTION = 81,


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

    /// Bulk (macro)porosity of the body
    BULK_POROSITY = 64,

    /// Heat capacity at constant pressure,
    HEAT_CAPACITY = 67,

    BODY_SHAPE_TYPE = 59,

    /// Center point of the body. Currently used only by \ref StabilizationSolver.
    BODY_CENTER = 61,

    BODY_RADIUS = 68,

    BODY_DIMENSIONS = 69,

    BODY_HEIGHT = 73,

    BODY_SPIN_RATE = 70,

    VISUALIZATION_TEXTURE = 80,

    /// Arbitrary string identifying this material
    IDENTIFIER = 99,
};

using RunSettings = Settings<RunSettingsId>;
using BodySettings = Settings<BodySettingsId>;

NAMESPACE_SPH_END
