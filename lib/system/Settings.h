#pragma once

/// Generic storage and input/output routines of settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Variant.h"
#include "objects/wrappers/Range.h"
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>

NAMESPACE_SPH_BEGIN

template <typename TEnum>
class Settings : public Object {
private:
    enum Types { BOOL, INT, FLOAT, RANGE, STRING, VECTOR };

    using Value = Variant<bool, int, float, Range, std::string, Vector>;

    struct Entry {
        TEnum id;
        std::string name;
        Value value;
    };

    std::map<TEnum, Entry> entries;

public:
    Settings() = default;

    Settings(std::initializer_list<Entry> list) {
        for (const Entry& entry : list) {
            entries[entry.id] = entry;
        }
    }

    Settings& operator=(std::initializer_list<Entry> list) {
        entries.clear();
        for (Entry& entry : list) {
            entries.insert(entry);
        }
    }

    template <typename TValue>
    void set(TEnum idx, TValue&& value) {
        entries[idx].value = std::forward<TValue>(value);
    }

    template <typename TValue>
    Optional<TValue> get(TEnum idx) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        if (iter == entries.end()) {
            return NOTHING;
        }
        return iter->second.value.get<TValue>();
    }

    void saveToFile(const std::string& path) const {
        std::ofstream ofs(path);
        for (auto& e : entries) {
            const Entry& entry = e.second;
            ofs << entry.name << " = ";
            switch (entry.value.getTypeIdx()) {
            case BOOL:
                ofs << (bool)entry.value;
                break;
            case INT:
                ofs << (int)entry.value;
                break;
            case FLOAT:
                ofs << (float)entry.value;
                break;
            case RANGE:
                ofs << entry.value.get<Range>().get();
                break;
            case STRING:
                ofs << entry.value.get<std::string>().get();
                break;
            case VECTOR:
                ofs << entry.value.get<Vector>().get();
            default:
                NOT_IMPLEMENTED;
            }
            ofs << std::endl;
        }
        ofs.close();
    }
};

enum class KernelEnum { CUBIC_SPLINE };

enum class KernelSymmetryEnum {
    /// Will return value of kernel at average smoothing lenghts
    SYMMETRIZE_LENGTHS,

    /// Average values of kernel for both smoothing lenghts
    SYMMETRIZE_KERNELS
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

enum class FinderEnum {
    /// Brute-force search by going through each pair of particles (O(N^2) complexity)
    BRUTE_FORCE,

    /// Optimized kNN queries using K-d tree
    KD_TREE,

    /// Optimized kNN queries using octree
    OCTREE,

    /// Optimized kNN queries using precomputed linked list
    LINKED_LIST
};

enum class BoundaryEnum {
    /// Do not use any boundary conditions (= vacuum conditions)
    NONE,

    /// Project outside particles onto the boundary
    DOMAIN_PROJECTING,

    /// Create ghosts to keep particles inside domain
    GHOST_PARTICLES,

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
    BLOCK
    // CYLINDER
};

/// Settings relevant for whole run of the simulation
enum class GlobalSettingsIds {
    /// Custom name of the run
    RUN_NAME,

    /// Frequency of dumping data to disk in number of timesteps
    RUN_OUTPUT_STEP,

    /// File name of the output file (including extension), where %d is a placeholder for output number.
    RUN_OUTPUT_NAME,

    /// Index of SPH Kernel, see KernelEnum
    SPH_KERNEL,

    /// Structure for searching nearest neighbours of particles
    SPH_FINDER,

    /// Boundary conditions
    BOUNDARY,

    /// Approach of smoothing length symmetrization, see SmoothingLengthEnum
    SPH_KERNEL_SYMMETRY,

    /// Selected timestepping integrator
    TIMESTEPPING_INTEGRATOR,

    /// Courant number
    TIMESTEPPING_COURANT,

    /// Upper limit of the time step
    TIMESTEPPING_MAX_TIMESTEP,

    /// Initial value of time step (relative to the maximal time step)
    TIMESTEPPING_INITIAL_TIMESTEP,

    /// Constant / adaptive time step
    TIMESTEPPING_ADAPTIVE,

    /// Multiplicative factor k in timestep computation; dt = k * v / dv
    TIMESTEPPING_ADAPTIVE_FACTOR,

    /// Artificial viscosity alpha coefficient
    AV_ALPHA,

    /// Artificial viscosity beta coefficient
    AV_BETA,

    /// Computational domain, enforced by boundary conditions
    DOMAIN_TYPE,

    /// Center point of the domain
    DOMAIN_CENTER,

    /// Radius of a spherical domain
    DOMAIN_RADIUS,

    /// (Vector) size of a block domain
    DOMAIN_SIZE,
};

// clang-format off
const Settings<GlobalSettingsIds> GLOBAL_SETTINGS = {
    { GlobalSettingsIds::RUN_NAME,                      "run.name",                 std::string("unnamed run") },
    { GlobalSettingsIds::RUN_OUTPUT_STEP,               "run.output_step",          100 },
    { GlobalSettingsIds::RUN_OUTPUT_NAME,               "run.output_name",          std::string("out_%d.txt") },
    { GlobalSettingsIds::SPH_KERNEL,                    "sph.kernel",               int(KernelEnum::CUBIC_SPLINE) },
    { GlobalSettingsIds::SPH_KERNEL_SYMMETRY,           "sph.kernel.symmetry",      int(KernelSymmetryEnum::SYMMETRIZE_LENGTHS) },
    { GlobalSettingsIds::SPH_FINDER,                    "sph.finder",               int(FinderEnum::KD_TREE) },
    { GlobalSettingsIds::TIMESTEPPING_INTEGRATOR,       "timestep.integrator",      int(TimesteppingEnum::EULER_EXPLICIT) },
    { GlobalSettingsIds::TIMESTEPPING_COURANT,          "timestep.courant",         1.f },
    { GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP,     "timestep.max_step",        0.1f /*s*/}, /// \todo units necessary in settings!!!
    { GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, "timestep.initial",         0.03f },
    { GlobalSettingsIds::TIMESTEPPING_ADAPTIVE,         "timestep.adaptive",        false },
    { GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR,  "timestep.adaptive.factor", 0.1f },
    { GlobalSettingsIds::AV_ALPHA,                      "av.alpha",                 1.5f },
    { GlobalSettingsIds::AV_BETA,                       "av.beta",                  3.f },
    { GlobalSettingsIds::BOUNDARY,                      "boundary",                 int(BoundaryEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_TYPE,                   "domain.type",              int(DomainEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_CENTER,                 "domain.center",            Vector(0.f) },
    { GlobalSettingsIds::DOMAIN_RADIUS,                 "domain.radius",            1.f },
    { GlobalSettingsIds::DOMAIN_SIZE,                   "domain.size",              Vector(1.f) },
};
// clang-format on


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
    /// Equation of state for ideal gas
    IDEAL_GAS,

    /// Tillotson (1962) equation of state
    TILLOTSON,

    /// ANEOS given by look-up table
    ANEOS
};


/// Settings of a single body / gas phase / ...
/// Combines material parameters and numerical parameters of the SPH method specific for one body.
enum class BodySettingsIds {
    /// Equation of state for this material
    EOS,

    /// Initial distribution of SPH particles within the domain
    INITIAL_DISTRIBUTION,

    /// Density at zero pressure
    DENSITY,

    DENSITY_RANGE,

    BULK_MODULUS,

    /// Initial energy
    ENERGY,

    ADIABATIC_INDEX,


    TILLOTSON_NONLINEAR_B,

    /// Specific energy of incipient vaporization
    TILLOTSON_ENERGY_IV,

    /// Specific energy of complete vaporization
    TILLOTSON_EVERGY_CV,

    SHEAR_MODULUS,

    VON_MISES_ELASTICITY_LIMIT,

    WEIBULL_COEFFICIENT,

    WEIBULL_EXPONENT,

    /// Number of SPH particles in the body
    PARTICLE_COUNT,
};

// clang-format off
const Settings<BodySettingsIds> BODY_SETTINGS = {
    { BodySettingsIds::EOS,                     "material.eos",                 int(EosEnum::IDEAL_GAS) },
    { BodySettingsIds::ADIABATIC_INDEX,         "material.eos.adiabatic_index", 1.5f },
    { BodySettingsIds::DENSITY,                 "material.density",             2700.f },
    { BodySettingsIds::DENSITY_RANGE,           "material.density.range",       Range(EPS, NOTHING) },
    { BodySettingsIds::ENERGY,                  "material.energy",              0.f },
    { BodySettingsIds::INITIAL_DISTRIBUTION,    "sph.initial_distribution",     int(DistributionEnum::HEXAGONAL) },
    { BodySettingsIds::PARTICLE_COUNT,          "sph.particle_count",           10000 },

};
// clang-format on


NAMESPACE_SPH_END
