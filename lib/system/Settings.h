#pragma once

/// Generic storage and input/output routines of settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Range.h"
#include "objects/wrappers/Variant.h"
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <map>
#include <regex>
#include <string>

NAMESPACE_SPH_BEGIN


template <typename TEnum>
class Settings : public Object {
private:
    enum Types { BOOL, INT, FLOAT, RANGE, STRING, VECTOR, TENSOR, TRACELESS_TENSOR };

    using Value = Variant<bool, int, Float, Range, std::string, Vector, Tensor, TracelessTensor>;

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
                ofs << (Float)entry.value;
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
            case TENSOR:
                ofs << entry.value.get<Tensor>().get();
                break;
            case TRACELESS_TENSOR:
                ofs << entry.value.get<TracelessTensor>().get();
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
            std::string key = line.substr(0, idx);
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
                        std::cout << "failed loading " << trimmedKey << std::endl;
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
            Float f;
            ss >> f;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = f;
                return true;
            }
        case RANGE: {
            std::string s1, s2;
            ss >> s1 >> s2;
            if (ss.fail()) {
                return false;
            }
            Optional<Float> lower, upper;
            if (s1 == "-infinity") {
                lower = NOTHING;
            } else {
                ss.clear();
                ss.str(s1);
                Float value;
                ss >> value;
                lower = value;
            }
            if (s2 == "infinity") {
                upper = NOTHING;
            } else {
                ss.clear();
                ss.str(s2);
                Float value;
                ss >> value;
                upper = value;
            }
            if (ss.fail()) {
                return false;
            } else {
                entry.value = Range(lower, upper);
                return true;
            }
        }
        case STRING: {
            // trim leading and trailing spaces
            const std::string trimmed = std::regex_replace(str, std::regex("^ +| +$|( ) +"), "$1");
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
        case TENSOR:
            Float sxx, syy, szz, sxy, sxz, syz;
            ss >> sxx >> syy >> szz >> sxy >> sxz >> syz;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = Tensor(Vector(sxx, syy, szz), Vector(sxy, sxz, syz));
                return true;
            }
        case TRACELESS_TENSOR:
            Float txx, tyy, txy, txz, tyz;
            ss >> txx >> tyy >> txy >> txz >> tyz;
            if (ss.fail()) {
                return false;
            } else {
                entry.value = TracelessTensor(txx, tyy, txy, txz, tyz);
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
    VON_MISES
};

enum class DamageEnum {
    /// No fragmentation
    NONE,

    /// Grady-Kipp model of fragmentation using scalar damage
    SCALAR_GRADY_KIPP,

    /// Grady-Kipp model of fragmentation using tensor damage
    TENSOR_GRADY_KIPP
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

    /// Eta-factor between smoothing length and particle concentration (h = eta * n^(-1/d) )
    SPH_KERNEL_ETA,

    /// Artificial viscosity alpha coefficient
    SPH_AV_ALPHA,

    /// Lower and upper bound of the alpha coefficient, used only for time-dependent artificial viscosity.
    SPH_AV_ALPHA_RANGE,

    /// Artificial viscosity beta coefficient
    SPH_AV_BETA,

    /// Use force from pressure gradient in the model
    MODEL_FORCE_GRAD_P,

    /// Use force from stress divergence in the model (must be used together with MODEL_FORCE_GRAD_P). Stress
    /// tensor is then evolved in time using Hooke's equation.
    MODEL_FORCE_DIV_S,

    /// Use centripetal force given by angular frequency of the coordinate frame in the model
    MODEL_FORCE_CENTRIPETAL,

    /// Use gravitational force in the model.
    MODEL_FORCE_GRAVITY,

    /// Type of used artificial viscosity.
    MODEL_AV_TYPE,

    /// Whether to use balsara switch for computing artificial viscosity dissipation. If no artificial
    /// viscosity is used, the value has no effect.
    MODEL_AV_BALSARA_SWITCH,

    /// Type of material yielding
    MODEL_YIELDING,

    /// Selected fragmentation model
    MODEL_DAMAGE,

    /// Selected solver for computing derivatives of physical variables.
    SOLVER_TYPE,

    /// Number of spatial dimensions of the problem.
    SOLVER_DIMENSIONS,

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

    /// Global rotation of the coordinate system around axis (0, 0, 1) passing through origin. If non-zero,
    /// causes non-intertial acceleration.
    FRAME_ANGULAR_FREQUENCY,

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
    /// Parameters of the run
    { GlobalSettingsIds::RUN_NAME,                      "run.name",                 std::string("unnamed run") },
    { GlobalSettingsIds::RUN_OUTPUT_STEP,               "run.output.step",          100 },
    { GlobalSettingsIds::RUN_OUTPUT_NAME,               "run.output.name",          std::string("out_%d.txt") },
    { GlobalSettingsIds::RUN_OUTPUT_PATH,               "run.output.path",          std::string("out") }, /// \todo Variant somehow doesnt handle empty strings

    /// Physical model
    { GlobalSettingsIds::MODEL_FORCE_GRAD_P,            "model.force.grad_p",       true },
    { GlobalSettingsIds::MODEL_FORCE_DIV_S,             "model.force.div_s",        true },
    { GlobalSettingsIds::MODEL_FORCE_CENTRIPETAL,       "model.force.centripetal",  false },
    { GlobalSettingsIds::MODEL_FORCE_GRAVITY,           "model.force.gravity",      false },
    { GlobalSettingsIds::MODEL_AV_TYPE,                 "model.av.type",            int(ArtificialViscosityEnum::STANDARD) },
    { GlobalSettingsIds::MODEL_AV_BALSARA_SWITCH,       "model.av.balsara_switch",  false },
    { GlobalSettingsIds::MODEL_YIELDING,                "model.yielding",           int(YieldingEnum::NONE) },
    { GlobalSettingsIds::MODEL_DAMAGE,                  "model.damage",             int(DamageEnum::NONE) },

    /// SPH solvers
    { GlobalSettingsIds::SOLVER_TYPE,                   "solver.type",              int (SolverEnum::CONTINUITY_SOLVER) },
    { GlobalSettingsIds::SOLVER_DIMENSIONS,             "solver.dimensions",        3 },

    /// Global SPH parameters
    { GlobalSettingsIds::SPH_KERNEL,                    "sph.kernel",               int(KernelEnum::CUBIC_SPLINE) },
    { GlobalSettingsIds::SPH_KERNEL_ETA,                "sph.kernel.eta",           1.5_f },
    { GlobalSettingsIds::SPH_AV_ALPHA,                  "sph.av.alpha",             1.5_f },
    { GlobalSettingsIds::SPH_AV_ALPHA_RANGE,            "sph.av.alpha.range",       Range(0.05_f, 1.5_f) },
    { GlobalSettingsIds::SPH_AV_BETA,                   "sph.av.beta",              3._f },
    { GlobalSettingsIds::SPH_FINDER,                    "sph.finder",               int(FinderEnum::KD_TREE) },

    /// Timestepping parameters
    { GlobalSettingsIds::TIMESTEPPING_INTEGRATOR,       "timestep.integrator",      int(TimesteppingEnum::EULER_EXPLICIT) },
    { GlobalSettingsIds::TIMESTEPPING_COURANT,          "timestep.courant",         1._f },
    { GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP,     "timestep.max_step",        0.1_f /*s*/}, /// \todo units necessary in settings!!!
    { GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, "timestep.initial",         0.03_f },
    { GlobalSettingsIds::TIMESTEPPING_ADAPTIVE,         "timestep.adaptive",        false },
    { GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR,  "timestep.adaptive.factor", 0.1_f },

    /// Selected coordinate system, rotation of bodies
    { GlobalSettingsIds::FRAME_ANGULAR_FREQUENCY,       "frame.angular_frequency",  0._f },

    /// Computational domain and boundary conditions
    { GlobalSettingsIds::DOMAIN_TYPE,                   "domain.type",              int(DomainEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_BOUNDARY,               "domain.boundary",          int(BoundaryEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_CENTER,                 "domain.center",            Vector(0._f) },
    { GlobalSettingsIds::DOMAIN_RADIUS,                 "domain.radius",            1._f },
    { GlobalSettingsIds::DOMAIN_SIZE,                   "domain.size",              Vector(1._f) },
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

    /// Initial values of the deviatoric stress tensor
    STRESS_TENSOR,

    /// Initial damage of the body.
    DAMAGE,

    /// Adiabatic index used by some equations of state (such as ideal gas)
    ADIABATIC_INDEX,

    /// Allowed range of damage
    DAMAGE_RANGE,

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

    /// Specific energy of incipient vaporization
    TILLOTSON_ENERGY_IV,

    /// Specific energy of complete vaporization
    TILLOTSON_ENERGY_CV,

    SHEAR_MODULUS,

    VON_MISES_ELASTICITY_LIMIT,

    /// Speed of crack growth, in units of local sound speed.
    RAYLEIGH_SOUND_SPEED,

    WEIBULL_COEFFICIENT,

    WEIBULL_EXPONENT,

    /// Number of SPH particles in the body
    PARTICLE_COUNT,
};

// clang-format off
const Settings<BodySettingsIds> BODY_SETTINGS = {
    /// Equation of state
    { BodySettingsIds::EOS,                     "eos",                          int(EosEnum::IDEAL_GAS) },
    { BodySettingsIds::ADIABATIC_INDEX,         "eos.adiabatic_index",          1.4_f },
    { BodySettingsIds::TILLOTSON_SMALL_A,       "eos.tillotson.small_a",        0.5_f },
    { BodySettingsIds::TILLOTSON_SMALL_B,       "eos.tillotson.small_b",        1.5_f },
    { BodySettingsIds::TILLOTSON_ALPHA,         "eos.tillotson.alpha",          5._f },
    { BodySettingsIds::TILLOTSON_BETA,          "eos.tillotson.beta",           5._f },
    { BodySettingsIds::TILLOTSON_NONLINEAR_B,   "eos.tillotson.nonlinear_b",    2.67e10_f },
    { BodySettingsIds::TILLOTSON_ENERGY_IV,     "eos.tillotson.energy_iv",      4.72e6_f },
    { BodySettingsIds::TILLOTSON_ENERGY_CV,     "eos.tillotson.energy_cv",      1.82e7_f },

    /// Material properties
    { BodySettingsIds::DENSITY,                 "material.density",             2700._f },
    { BodySettingsIds::DENSITY_RANGE,           "material.density.range",       Range(1._f, NOTHING) },
    { BodySettingsIds::ENERGY,                  "material.energy",              0._f },
    { BodySettingsIds::ENERGY_RANGE,            "material.energy.range",        Range(0._f, NOTHING) },
    { BodySettingsIds::DAMAGE,                  "material.damage",              0._f },
    { BodySettingsIds::DAMAGE_RANGE,            "material.damage.range",        Range(0.f, 1._f) },
    { BodySettingsIds::STRESS_TENSOR,           "material.stress_tensor",       TracelessTensor(0._f) },
    { BodySettingsIds::BULK_MODULUS,            "material.bulk_modulus",        2.67e10_f },
    { BodySettingsIds::SHEAR_MODULUS,           "material.shear_modulus",       2.27e10_f },
    { BodySettingsIds::RAYLEIGH_SOUND_SPEED,    "material.rayleigh_speed",      0.4_f },
    { BodySettingsIds::WEIBULL_COEFFICIENT,     "material.weibull_coefficient", 4.e23_f },
    { BodySettingsIds::WEIBULL_EXPONENT,        "material.weibull_exponent",    9._f },

    /// SPH parameters specific for the body
    { BodySettingsIds::INITIAL_DISTRIBUTION,    "sph.initial_distribution",     int(DistributionEnum::HEXAGONAL) },
    { BodySettingsIds::PARTICLE_COUNT,          "sph.particle_count",           10000 },
};
// clang-format on

using GlobalSettings = Settings<GlobalSettingsIds>;
using BodySettings = Settings<BodySettingsIds>;


NAMESPACE_SPH_END
