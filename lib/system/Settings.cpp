#include "system/Settings.h"
#include <fstream>
#include <regex>

NAMESPACE_SPH_BEGIN

template <typename TEnum>
void Settings<TEnum>::saveToFile(const std::string& path) const {
    std::ofstream ofs(path);
    for (auto& e : entries) {
        const Entry& entry = e.second;
        ofs << std::setw(30) << std::left << entry.name << " = ";
        switch (entry.value.getTypeIdx()) {
        case BOOL:
            ofs << entry.value.template get<bool>();
            break;
        case INT:
            ofs << entry.value.template get<int>();
            break;
        case FLOAT:
            ofs << entry.value.template get<Float>();
            break;
        case RANGE:
            ofs << entry.value.template get<Range>();
            break;
        case STRING:
            ofs << entry.value.template get<std::string>();
            break;
        case VECTOR:
            ofs << entry.value.template get<Vector>();
            break;
        case TENSOR:
            ofs << entry.value.template get<Tensor>();
            break;
        case TRACELESS_TENSOR:
            ofs << entry.value.template get<TracelessTensor>();
            break;
        default:
            NOT_IMPLEMENTED;
        }
        ofs << std::endl;
    }
    ofs.close();
}

template <typename TEnum>
bool Settings<TEnum>::loadFromFile(const std::string& path, const Settings& descriptors) {
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
                    /// \todo logger
                    /// std::cout << "failed loading " << trimmedKey << std::endl;
                    return false;
                }
            }
        }
    }
    ifs.close();
    return true;
}

template <typename TEnum>
bool Settings<TEnum>::setValueByType(Entry& entry, const Size typeIdx, const std::string& str) {
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
        Float lower, upper;
        if (s1 == "-infinity") {
            lower = -INFTY;
        } else {
            ss.clear();
            ss.str(s1);
            Float value;
            ss >> value;
            lower = value;
        }
        if (s2 == "infinity") {
            upper = INFTY;
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

// Explicit instantiation
template class Settings<BodySettingsIds>;
template class Settings<GlobalSettingsIds>;


// clang-format off
template<>
std::unique_ptr<GlobalSettings> GlobalSettings::instance (new GlobalSettings {
    /// Parameters of the run
    { GlobalSettingsIds::RUN_NAME,                      "run.name",                 std::string("unnamed run") },
    { GlobalSettingsIds::RUN_OUTPUT_INTERVAL,           "run.output.interval",      0.01_f },
    { GlobalSettingsIds::RUN_OUTPUT_NAME,               "run.output.name",          std::string("out_%d.txt") },
    { GlobalSettingsIds::RUN_OUTPUT_PATH,               "run.output.path",          std::string("out") },
    { GlobalSettingsIds::RUN_LOGGER,                    "run.logger",               int(LoggerEnum::STD_OUT) },
    { GlobalSettingsIds::RUN_LOGGER_FILE,               "run.logger.file",          std::string("log.txt") },
    { GlobalSettingsIds::RUN_STATISTICS_STEP,           "run.statistics_step",      100 },

    /// Physical model
    { GlobalSettingsIds::MODEL_FORCE_GRAD_P,            "model.force.grad_p",       true },
    { GlobalSettingsIds::MODEL_FORCE_DIV_S,             "model.force.div_s",        true },
    { GlobalSettingsIds::MODEL_FORCE_CENTRIPETAL,       "model.force.centripetal",  false },
    { GlobalSettingsIds::MODEL_FORCE_GRAVITY,           "model.force.gravity",      false },
    { GlobalSettingsIds::MODEL_AV_TYPE,                 "model.av.type",            int(ArtificialViscosityEnum::STANDARD) },
    { GlobalSettingsIds::MODEL_AV_BALSARA_SWITCH,       "model.av.balsara_switch",  false },
    { GlobalSettingsIds::MODEL_YIELDING,                "model.yielding",           int(YieldingEnum::VON_MISES) },
    { GlobalSettingsIds::MODEL_DAMAGE,                  "model.damage",             int(DamageEnum::NONE) },

    /// SPH solvers
    { GlobalSettingsIds::SOLVER_TYPE,                   "solver.type",              int(SolverEnum::CONTINUITY_SOLVER) },

    /// Global SPH parameters
    { GlobalSettingsIds::SPH_KERNEL,                    "sph.kernel",               int(KernelEnum::CUBIC_SPLINE) },
    { GlobalSettingsIds::SPH_KERNEL_ETA,                "sph.kernel.eta",           1.5_f },
    { GlobalSettingsIds::SPH_AV_ALPHA,                  "sph.av.alpha",             1.5_f },
    { GlobalSettingsIds::SPH_AV_BETA,                   "sph.av.beta",              3._f },
    { GlobalSettingsIds::SPH_SMOOTHING_LENGTH_MIN,      "sph.smoothing_length.min", 1e-5_f },
    { GlobalSettingsIds::SPH_FINDER,                    "sph.finder",               int(FinderEnum::VOXEL) },

    /// Timestepping parameters
    { GlobalSettingsIds::TIMESTEPPING_INTEGRATOR,       "timestep.integrator",      int(TimesteppingEnum::PREDICTOR_CORRECTOR) },
    { GlobalSettingsIds::TIMESTEPPING_COURANT,          "timestep.courant",         1._f },
    { GlobalSettingsIds::TIMESTEPPING_MAX_TIMESTEP,     "timestep.max_step",        0.1_f /*s*/}, /// \todo units necessary in settings!!!
    { GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, "timestep.initial",         0.03_f },
    { GlobalSettingsIds::TIMESTEPPING_CRITERION,        "timestep.criterion",       int(TimeStepCriterionEnum::ALL) },
    { GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR,  "timestep.adaptive.factor", 0.2_f },

    /// Selected coordinate system, rotation of bodies
    { GlobalSettingsIds::FRAME_ANGULAR_FREQUENCY,       "frame.angular_frequency",  0._f },

    /// Computational domain and boundary conditions
    { GlobalSettingsIds::DOMAIN_TYPE,                   "domain.type",              int(DomainEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_BOUNDARY,               "domain.boundary",          int(BoundaryEnum::NONE) },
    { GlobalSettingsIds::DOMAIN_GHOST_MIN_DIST,         "domain.ghosts.min_dist",   0.1_f },
    { GlobalSettingsIds::DOMAIN_CENTER,                 "domain.center",            Vector(0._f) },
    { GlobalSettingsIds::DOMAIN_RADIUS,                 "domain.radius",            1._f },
    { GlobalSettingsIds::DOMAIN_HEIGHT,                 "domain.height",            1._f },
    { GlobalSettingsIds::DOMAIN_SIZE,                   "domain.size",              Vector(1._f) },
});
// clang-format on


// clang-format off
template<>
std::unique_ptr<BodySettings> BodySettings::instance (new BodySettings {
    /// Equation of state
    { BodySettingsIds::EOS,                     "eos",                          int(EosEnum::IDEAL_GAS) },
    { BodySettingsIds::ADIABATIC_INDEX,         "eos.adiabatic_index",          1.4_f },
    { BodySettingsIds::TILLOTSON_SMALL_A,       "eos.tillotson.small_a",        0.5_f },
    { BodySettingsIds::TILLOTSON_SMALL_B,       "eos.tillotson.small_b",        1.5_f },
    { BodySettingsIds::TILLOTSON_ALPHA,         "eos.tillotson.alpha",          5._f },
    { BodySettingsIds::TILLOTSON_BETA,          "eos.tillotson.beta",           5._f },
    { BodySettingsIds::TILLOTSON_NONLINEAR_B,   "eos.tillotson.nonlinear_b",    2.67e10_f },
    { BodySettingsIds::TILLOTSON_SUBLIMATION,   "eos.tillotson.sublimation",    4.87e8_f },
    { BodySettingsIds::TILLOTSON_ENERGY_IV,     "eos.tillotson.energy_iv",      4.72e6_f },
    { BodySettingsIds::TILLOTSON_ENERGY_CV,     "eos.tillotson.energy_cv",      1.82e7_f },

    /// Yielding & Damage
    { BodySettingsIds::ELASTICITY_LIMIT,        "rheology.elasticity_limit",    3.5e9_f },
    { BodySettingsIds::MELT_ENERGY,             "rheology.melt_energy",         3.4e6_f },
    { BodySettingsIds::COHESION,                "rheology.cohesion",            9.e7_f },
    { BodySettingsIds::INTERNAL_FRICTION,       "rheology.internal_friction",   2._f },
    { BodySettingsIds::DRY_FRICTION,            "rheology.dry_friction",        0.8_f },

    /// Material properties
    { BodySettingsIds::DENSITY,                 "material.density",             2700._f },
    { BodySettingsIds::DENSITY_RANGE,           "material.density.range",       Range(10._f, INFTY) },
    { BodySettingsIds::DENSITY_MIN,             "material.density.min",         50._f },
    { BodySettingsIds::ENERGY,                  "material.energy",              0._f },
    { BodySettingsIds::ENERGY_RANGE,            "material.energy.range",        Range(0._f, INFTY) },
    { BodySettingsIds::ENERGY_MIN,              "material.energy.min",          1._f },
    { BodySettingsIds::DAMAGE,                  "material.damage",              0._f },
    { BodySettingsIds::DAMAGE_RANGE,            "material.damage.range",        Range(0.f, 1._f) },
    { BodySettingsIds::DAMAGE_MIN,              "material.damage.min",          0.03_f },
    { BodySettingsIds::STRESS_TENSOR,           "material.stress_tensor",       TracelessTensor(0._f) },
    { BodySettingsIds::STRESS_TENSOR_MIN,       "material.stress_tensor.min",   1e5_f },
    { BodySettingsIds::BULK_MODULUS,            "material.bulk_modulus",        2.67e10_f },
    { BodySettingsIds::SHEAR_MODULUS,           "material.shear_modulus",       2.27e10_f },
    { BodySettingsIds::YOUNG_MODULUS,           "material.young_modulus",       5.7e10_f },
    { BodySettingsIds::RAYLEIGH_SOUND_SPEED,    "material.rayleigh_speed",      0.4_f },
    { BodySettingsIds::WEIBULL_COEFFICIENT,     "material.weibull_coefficient", 4.e35_f },
    { BodySettingsIds::WEIBULL_EXPONENT,        "material.weibull_exponent",    9._f },

    /// SPH parameters specific for the body
    { BodySettingsIds::INITIAL_DISTRIBUTION,    "sph.initial_distribution",     int(DistributionEnum::HEXAGONAL) },
    { BodySettingsIds::PARTICLE_SORTING,        "sph.particle_sorting",         false },
    { BodySettingsIds::PARTICLE_COUNT,          "sph.particle_count",           10000 },
    { BodySettingsIds::AV_ALPHA,                "av.alpha",                     1.5_f },
    { BodySettingsIds::AV_ALPHA_RANGE,          "av.alpha.range",               Range(0.05_f, 1.5_f) },
    { BodySettingsIds::AV_BETA,                 "av.beta",                      3._f },
    { BodySettingsIds::AV_ALPHA_RANGE,          "av.beta.range",                Range(0.1_f, 3._f) },
});
// clang-format on

NAMESPACE_SPH_END
