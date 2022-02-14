#include "run/jobs/MaterialJobs.h"
#include "physics/Aneos.h"
#include "sph/Materials.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// MaterialProvider
// ----------------------------------------------------------------------------------------------------------

MaterialProvider::MaterialProvider(const BodySettings& overrides) {
    body.set(BodySettingsId::ENERGY, 1.e3_f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 4.e6_f)
        .set(BodySettingsId::ENERGY_MIN, 10._f)
        .set(BodySettingsId::DAMAGE_MIN, 0.25_f);

    body.addEntries(overrides);
}

void MaterialProvider::addMaterialEntries(VirtualSettings::Category& category, Function<bool()> enabler) {
    auto enablerDp = [this, enabler] {
        const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
        return (!enabler || enabler()) && id == YieldingEnum::DRUCKER_PRAGER;
    };
    auto enablerAf = [this, enabler] {
        const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
        const bool use = body.get<bool>(BodySettingsId::USE_ACOUSTIC_FLUDIZATION);
        return (!enabler || enabler()) && use && id == YieldingEnum::DRUCKER_PRAGER;
    };
    auto enablerRheo = [this, enabler] {
        const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
        return (!enabler || enabler()) && (id != YieldingEnum::NONE && id != YieldingEnum::DUST);
    };
    auto enablerFrag = [this, enablerRheo] {
        const FractureEnum id = body.get<FractureEnum>(BodySettingsId::RHEOLOGY_DAMAGE);
        return enablerRheo() && id != FractureEnum::NONE;
    };

    category.connect<EnumWrapper>("Equation of state", body, BodySettingsId::EOS).setEnabler(enabler);
    category.connect<Float>("Density [kg/m^3]", body, BodySettingsId::DENSITY).setEnabler([this, enabler] {
        const EosEnum id = body.get<EosEnum>(BodySettingsId::EOS);
        return (!enabler || enabler()) && id != EosEnum::ANEOS;
    });
    category.connect<Float>("Specific energy [J/kg]", body, BodySettingsId::ENERGY).setEnabler(enabler);
    category.connect<Float>("Adiabatic index []", body, BodySettingsId::ADIABATIC_INDEX)
        .setEnabler([this, enabler] {
            const EosEnum eos = body.get<EosEnum>(BodySettingsId::EOS);
            return (!enabler || enabler()) && eos == EosEnum::IDEAL_GAS;
        })
        .setValidator([](const IVirtualEntry::Value& gamma) { return gamma.get<Float>() > 1.01_f; });
    category.connect<Float>("Damage []", body, BodySettingsId::DAMAGE).setEnabler(enabler);
    category.connect<EnumWrapper>("Rheology", body, BodySettingsId::RHEOLOGY_YIELDING).setEnabler(enabler);
    category.connect<Float>("Bulk modulus [Pa]", body, BodySettingsId::BULK_MODULUS)
        .setEnabler([this, enabler] {
            const EosEnum eos = body.get<EosEnum>(BodySettingsId::EOS);
            const YieldingEnum yield = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
            return (!enabler || enabler()) &&
                   ((eos != EosEnum::NONE && eos != EosEnum::IDEAL_GAS) ||
                       (yield != YieldingEnum::NONE && yield != YieldingEnum::DUST));
        });
    category.connect<Float>("Shear modulus [Pa]", body, BodySettingsId::SHEAR_MODULUS)
        .setEnabler(enablerRheo);
    category.connect<Float>("Elastic modulus [Pa]", body, BodySettingsId::ELASTIC_MODULUS)
        .setEnabler(enablerRheo);
    category.connect<Float>("von Mises limit [Pa]", body, BodySettingsId::ELASTICITY_LIMIT)
        .setEnabler([this, enabler] {
            const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
            return (!enabler || enabler()) &&
                   (id == YieldingEnum::VON_MISES || id == YieldingEnum::DRUCKER_PRAGER);
        });
    category.connect<Float>("Melting energy [J/kg]", body, BodySettingsId::MELT_ENERGY)
        .setEnabler(enablerRheo);
    category.connect<Float>("Internal friction []", body, BodySettingsId::INTERNAL_FRICTION)
        .setEnabler(enablerDp);
    category.connect<Float>("Cohesion [Pa]", body, BodySettingsId::COHESION).setEnabler(enablerDp);
    category.connect<Float>("Dry friction []", body, BodySettingsId::DRY_FRICTION).setEnabler(enablerDp);
    category.connect<bool>("Use acoustic fludization", body, BodySettingsId::USE_ACOUSTIC_FLUDIZATION)
        .setEnabler(enablerDp);
    category.connect<Float>("Oscillation decay time [s]", body, BodySettingsId::OSCILLATION_DECAY_TIME)
        .setEnabler(enablerAf);
    category.connect<Float>("Oscillation regeneration []", body, BodySettingsId::OSCILLATION_REGENERATION)
        .setEnabler(enablerAf);
    category.connect<Float>("Fludization viscosity", body, BodySettingsId::FLUIDIZATION_VISCOSITY)
        .setEnabler(enablerAf);
    category.connect<EnumWrapper>("Fragmentation", body, BodySettingsId::RHEOLOGY_DAMAGE)
        .setEnabler(enablerRheo);
    category.connect<Float>("Weibull exponent", body, BodySettingsId::WEIBULL_EXPONENT)
        .setEnabler(enablerFrag);
    category.connect<Float>("Weibull coefficient", body, BodySettingsId::WEIBULL_COEFFICIENT)
        .setEnabler(enablerFrag);
    category.connect<bool>("Sample distributions", body, BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS)
        .setEnabler(enablerFrag);
}

// ----------------------------------------------------------------------------------------------------------
// MaterialJob
// ----------------------------------------------------------------------------------------------------------

MaterialJob::MaterialJob(const String& name, const BodySettings& overrides)
    : IMaterialJob(name)
    , MaterialProvider(overrides) {}

VirtualSettings MaterialJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& materialCat = connector.addCategory("Material");
    this->addMaterialEntries(materialCat, nullptr);

    auto tillotsonEnabler = [this] { return body.get<EosEnum>(BodySettingsId::EOS) == EosEnum::TILLOTSON; };

    VirtualSettings::Category& tillotsonCat = connector.addCategory("Tillotson's parameters");
    tillotsonCat.connect<Float>("Tillotson's a", body, BodySettingsId::TILLOTSON_SMALL_A)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Tillotson's b", body, BodySettingsId::TILLOTSON_SMALL_B)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Tillotson's B", body, BodySettingsId::TILLOTSON_NONLINEAR_B)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Tillotson's alpha", body, BodySettingsId::TILLOTSON_ALPHA)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Tillotson's beta", body, BodySettingsId::TILLOTSON_BETA)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Incipient vaporization energy", body, BodySettingsId::TILLOTSON_ENERGY_IV)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Complete vaporization energy", body, BodySettingsId::TILLOTSON_ENERGY_CV)
        .setEnabler(tillotsonEnabler);
    tillotsonCat.connect<Float>("Sublimation energy", body, BodySettingsId::TILLOTSON_SUBLIMATION)
        .setEnabler(tillotsonEnabler);

    VirtualSettings::Category& aneosCat = connector.addCategory("ANEOS");
    aneosCat.connect<Path>("ANEOS material file", body, BodySettingsId::ANEOS_FILE)
        .setEnabler([this] { return body.get<EosEnum>(BodySettingsId::EOS) == EosEnum::ANEOS; })
        .setPathType(IVirtualEntry::PathType::INPUT_FILE);

    VirtualSettings::Category& integratorCat = connector.addCategory("Time step control");
    integratorCat.connect<Float>("Density coeff. [kg/m^3]", body, BodySettingsId::DENSITY_MIN);
    integratorCat.connect<Float>("Energy coeff. [J/kg]", body, BodySettingsId::ENERGY_MIN);
    integratorCat.connect<Float>("Stress coeff. [Pa]", body, BodySettingsId::STRESS_TENSOR_MIN);
    integratorCat.connect<Float>("Damage coeff. []", body, BodySettingsId::DAMAGE_MIN);

    return connector;
}

void MaterialJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    if (body.get<EosEnum>(BodySettingsId::EOS) == EosEnum::ANEOS) {
        // override the initial density
        const Path path(body.get<String>(BodySettingsId::ANEOS_FILE));
        const Float rho0 = getInitialDensity(parseAneosFile(path));
        body.set(BodySettingsId::DENSITY, rho0);
    }
    body.set(BodySettingsId::IDENTIFIER, instName);
    result = Factory::getMaterial(body);
}

static JobRegistrar sRegisterMaterial(
    "material",
    "materials",
    [](const String& name) { return makeAuto<MaterialJob>(name); },
    "Generic material");

// these presets only differ in initial parameters, so it's ok if they have different class names
static JobRegistrar sRegisterBasalt(
    "basalt",
    "materials",
    [](const String& name) {
        return makeAuto<MaterialJob>(name, getMaterial(MaterialEnum::BASALT)->getParams());
    },
    "Basalt");
static JobRegistrar sRegisterIce(
    "ice",
    "materials",
    [](const String& name) {
        return makeAuto<MaterialJob>(name, getMaterial(MaterialEnum::ICE)->getParams());
    },
    "Ice");
static JobRegistrar sRegisterOlivine(
    "olivine",
    "materials",
    [](const String& name) {
        return makeAuto<MaterialJob>(name, getMaterial(MaterialEnum::OLIVINE)->getParams());
    },
    "Olivine");
static JobRegistrar sRegisterIron(
    "iron",
    "materials",
    [](const String& name) {
        return makeAuto<MaterialJob>(name, getMaterial(MaterialEnum::IRON)->getParams());
    },
    "Iron");
static JobRegistrar sRegisterGas(
    "hydrogen gas",
    "materials",
    [](const String& name) {
        return makeAuto<MaterialJob>(name, getMaterial(MaterialEnum::HYDROGEN_GAS)->getParams());
    },
    "Basalt");

// ----------------------------------------------------------------------------------------------------------
// DisableDerivativeCriterionJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings DisableDerivativeCriterionJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void DisableDerivativeCriterionJob::evaluate(const RunSettings& UNUSED(global),
    IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IMaterial> input = this->getInput<IMaterial>("material");

    // basically should clone the material, needs to be generalized if more complex material setups are used
    result = Factory::getMaterial(input->getParams());
    result->setParam(BodySettingsId::STRESS_TENSOR_MIN, LARGE);
    result->setParam(BodySettingsId::DAMAGE_MIN, LARGE);
}

static JobRegistrar sRegisterDisabler(
    "optimize timestepping",
    "optimizer",
    "materials",
    [](const String& name) { return makeAuto<DisableDerivativeCriterionJob>(name); },
    "Helper material modifier that turns off the time step limitation for damage and stress "
    "tensor. Useful to avoid very low time steps due to particles that are deemed not important to "
    "the solution (such as impactor particles). If the time step is not limited by the derivative "
    "criterion, this material modifier simply forwards the material parameters unchanged.");

NAMESPACE_SPH_END
