#pragma once

/// Generic storage and input/output routines of settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/wrappers/Variant.h"
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>

NAMESPACE_SPH_BEGIN

template <typename TEnum>
class Settings : public Object {
private:
    enum Types { BOOL, INT, FLOAT, STRING, VECTOR };

    using Value = Variant<bool, int, float, std::string, Vector>;

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
        entries[idx] = std::forward<TValue>(value);
    }

    template <typename TValue>
    Optional<TValue> get(TEnum idx) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        if (iter == entries.end()) {
            return NOTHING;
        }
        return (TValue)iter->second.value;
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
            case STRING:
                ofs << entry.value.get<std::string>().get();
                break;
            case VECTOR:
                ofs << entry.value.get<Vector>().get();
            default:
                ASSERT(false);
            }
            ofs << std::endl;
        }
        ofs.close();
    }
};

enum class KernelEnum { CUBIC_SPLINE };

enum class SmoothingLengthEnum {
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

enum class EosEnum {
    /// Equation of state for ideal gas
    IDEAL_GAS,

    /// Tillotson (1962) equation of state
    TILLOTSON,

    /// ANEOS given by look-up table
    ANEOS
};

enum class FinderEnum {
    /// Brute-force search by going through each pair of particles (O(N^2) complexity)
    BRUTE_FORCE,

    /// Optimized kNN queries using K-d tree
    KD_TREE,

    /// Optimized kNN queries using precomputed linked list
    LINKED_LIST
};


/// Settings relevant for whole run of the simulation
enum class GlobalSettingsIds {
    /// Custom name of the run
    RUN_NAME,

    /// Index of SPH Kernel, see KernelEnum
    KERNEL,

    /// Structure for searching nearest neighbours of particles
    FINDER,

    /// Approach of smoothing length symmetrization, see SmoothingLengthEnum
    SMOOTHING_LENGTH_SYMMETRIZATION,

    /// Selected timestepping integrator
    TIMESTEPPING_INTEGRATOR,

    /// Courant number
    TIMESTEPPING_COURANT,

    /// Upper limit of the time step
    TIMESTEPPING_MAX_TIMESTEP,

    /// Initial value of time step (relative to the maximal time step)
    TIMESTEPPING_INITIAL_TIMESTEP,

    /// Artificial viscosity alpha coefficient
    AV_ALPHA,

    /// Artificial viscosity beta coefficient
    AV_BETA,
};

// clang-format off
const Settings<GlobalSettingsIds> GLOBAL_SETTINGS = {
    { GlobalSettingsIds::RUN_NAME,                          "run.name",             std::string("unnamed run") },
    { GlobalSettingsIds::KERNEL,                            "sph.kernel",           int(KernelEnum::CUBIC_SPLINE) },
    { GlobalSettingsIds::SMOOTHING_LENGTH_SYMMETRIZATION,   "sph.h_sym",            int(SmoothingLengthEnum::SYMMETRIZE_LENGTHS) },
    { GlobalSettingsIds::FINDER,                            "sph.finder",           int(FinderEnum::KD_TREE) },
    { GlobalSettingsIds::TIMESTEPPING_INTEGRATOR,           "timestep.integrator",  int(TimesteppingEnum::EULER_EXPLICIT) },
    { GlobalSettingsIds::TIMESTEPPING_COURANT,              "timestep.courant",     1.f },
    { GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP,         "timestep.max_step",    0.1f /*s*/}, /// \todo units necessary in settings!!!
    { GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP,     "timestep.initial",     0.03f },
    { GlobalSettingsIds::AV_ALPHA,                          "av.alpha",             1.5f },
    { GlobalSettingsIds::AV_BETA,                           "av.beta",              3.f },
};
// clang-format on


/// Settings of a single body / gas phase / ...
/// Combines material parameters and numerical parameters of the SPH method specific for one body.
enum class BodySettingsIds {
    /// Equation of state for this material
    EOS,

    /// Density at zero pressure
    DENSITY,

    BULK_MODULUS,

    NONLINEAR_TILLOTSON_B,

    /// Specific energy of incipient vaporization
    ENERGY_IV,

    /// Specific energy of complete vaporization
    EVERGY_CV,

    SHEAR_MODULUS,

    VON_MISES_ELASTICITY_LIMIT,

    WEIBULL_COEFFICIENT,

    WEIBULL_EXPONENT,

    /// Number of SPH particles in the body
    PARTICLE_COUNT,
};

// clang-format off
const Settings<BodySettingsIds> BODY_SETTINGS = {
    { BodySettingsIds::EOS,             "material.eos",         int(EosEnum::IDEAL_GAS) },
    { BodySettingsIds::DENSITY,         "material.density",     2700.f },
    { BodySettingsIds::PARTICLE_COUNT,  "sph.particle_count",   10000 },
};
// clang-format on


NAMESPACE_SPH_END
