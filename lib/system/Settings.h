#pragma once

/// Generic storage and input/output routines of settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Range.h"
#include "objects/wrappers/Variant.h"
#include <fstream>
#include <initializer_list>
#include <map>
#include <regex>
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
        using StoreType = EnumToInt<TValue>;
        entries[idx].value = StoreType(std::forward<TValue>(value));
    }

    template <typename TValue>
    TValue get(TEnum idx) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        ASSERT(iter != entries.end());
        /// \todo can be cast here as we no longer return optional
        using StoreType = EnumToInt<TValue>;
        auto opt = iter->second.value.get<StoreType>();
        ASSERT(opt);
        return TValue(opt.get());
    }

    void saveToFile(const std::string& path) const {
        std::ofstream ofs(path);
        for (auto& e : entries) {
            const Entry& entry = e.second;
            ofs << std::setw(30) << std::left << entry.name << " = ";
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
                break;
            default:
                NOT_IMPLEMENTED;
            }
            ofs << std::endl;
        }
        ofs.close();
    }

    /// \todo split settings and descriptors? Settings actual object with values, descriptors global object
    /// with ids, names and default values.
    bool loadFromFile(const std::string& path, const Settings& descriptors) {
        std::ifstream ifs(path);
        std::string line;
        while (std::getline(ifs, line, '\n')) {
            std::string::size_type idx = line.find("=");
            if (idx == std::string::npos) {
                // didn't find '=', invalid format of the file
                return false;
            }
            std::string key   = line.substr(0, idx);
            std::string value = line.substr(idx + 1);
            // throw away spaces from key
            std::string trimmedKey;
            for (const char c : key) {
                if (c != ' ') {
                    trimmedKey.push_back(c);
                }
            }
            // find the key in decriptor settings
            for (auto&& e : descriptors.entries) {
                if (e.second.name == trimmedKey) {
                    if (!setValueByType(this->entries[e.second.id], e.second.value.getTypeIdx(), value)) {
                        return false;
                    }
                }
            }
        }
        ifs.close();
        return true;
    }

private:
    bool setValueByType(Entry& entry, const int typeIdx, const std::string& str) {
        std::stringstream ss(str);
        switch (typeIdx) {
        case BOOL: {
            bool b;
            ss >> b;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = b;
                return true;
            }
        }
        case INT:
            int i;
            ss >> i;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = i;
                return true;
            }
        case FLOAT:
            float f;
            ss >> f;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = f;
                return true;
            }
        case RANGE:
            Float f1, f2;
            ss >> f1 >> f2;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = Range(f1, f2);
                return true;
            }
        case STRING: {
            // trim leading and trailing spaces
            const std::string trimmed  =std::regex_replace(str, std::regex("^ +| +$|( ) +"), "$1");
            ASSERT(!trimmed.empty() && "Variant cannot handle empty strings");
            entry.value = trimmed;
            return true;
        }
        case VECTOR:
            Float v1, v2, v3;
            ss >> v1 >> v2 >> v3;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = Vector(v1, v2, v3);
                return true;
            }
        default:
            NOT_IMPLEMENTED;
        }
    }
};

enum class KernelEnum {
    /// M4 B-spline (piecewise cubic polynomial)
    CUBIC_SPLINE,

    /// M5 B-spline (piecewise 4th-order polynomial)
    FOURTH_ORDER_SPLINE,
};

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

    /// Path where all output files (dumps, logs, ...) will be written
    RUN_OUTPUT_PATH,

    /// Index of SPH Kernel, see KernelEnum
    SPH_KERNEL,

    /// Structure for searching nearest neighbours of particles
    SPH_FINDER,

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

    /// Type of boundary conditions.
    DOMAIN_BOUNDARY,

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
    { GlobalSettingsIds::RUN_OUTPUT_STEP,               "run.output.step",          100 },
    { GlobalSettingsIds::RUN_OUTPUT_NAME,               "run.output.name",          std::string("out_%d.txt") },
    { GlobalSettingsIds::RUN_OUTPUT_PATH,               "run.output.path",          std::string("out") }, /// \todo Variant somehow doesnt handle empty strings
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
    { GlobalSettingsIds::DOMAIN_TYPE,                   "domain.type",              int(DomainEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_BOUNDARY,               "domain.boundary",          int(BoundaryEnum::NONE) },
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

    /// Allowed range of density. Densities of all particles all clamped to fit in the range.
    DENSITY_RANGE,

    /// Initial specific internal energy
    ENERGY,

    /// Allowed range of specific internal energy.
    ENERGY_RANGE,

    /// Initial damage of the body.
    DAMAGE,

    /// Allowed range of damage.
    DAMAGE_RANGE,

    BULK_MODULUS,

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
    { BodySettingsIds::EOS,                     "eos",                          int(EosEnum::IDEAL_GAS) },
    { BodySettingsIds::ADIABATIC_INDEX,         "eos.adiabatic_index",          1.4f },
    { BodySettingsIds::DENSITY,                 "material.density",             2700.f },
    { BodySettingsIds::DENSITY_RANGE,           "material.density.range",       Range(1.f, NOTHING) },
    { BodySettingsIds::ENERGY,                  "material.energy",              0.f },
    { BodySettingsIds::ENERGY_RANGE,            "material.energy.range",        Range(0.f, NOTHING) },
    { BodySettingsIds::DAMAGE,                  "material.damage",              0.f },
    { BodySettingsIds::DAMAGE_RANGE,            "material.damage.range",        Range(0.f, 1.f) },
    { BodySettingsIds::INITIAL_DISTRIBUTION,    "sph.initial_distribution",     int(DistributionEnum::HEXAGONAL) },
    { BodySettingsIds::PARTICLE_COUNT,          "sph.particle_count",           10000 },

};
// clang-format on


NAMESPACE_SPH_END
