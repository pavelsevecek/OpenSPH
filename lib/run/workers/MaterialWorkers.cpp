#include "run/workers/MaterialWorkers.h"
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

    category.connect<EnumWrapper>("EoS", body, BodySettingsId::EOS).setEnabler(enabler);
    category.connect<Float>("Density [kg/m^3]", body, BodySettingsId::DENSITY).setEnabler(enabler);
    category.connect<Float>("Specific energy [J/kg]", body, BodySettingsId::ENERGY).setEnabler(enabler);
    category.connect<Float>("Damage []", body, BodySettingsId::DAMAGE).setEnabler(enabler);
    category.connect<EnumWrapper>("Rheology", body, BodySettingsId::RHEOLOGY_YIELDING).setEnabler(enabler);
    category.connect<Float>("Shear modulur [Pa]", body, BodySettingsId::SHEAR_MODULUS)
        .setEnabler([this, enabler] {
            const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
            return (!enabler || enabler()) && id != YieldingEnum::NONE;
        });
    category.connect<Float>("von Mises limit [Pa]", body, BodySettingsId::ELASTICITY_LIMIT)
        .setEnabler([this, enabler] {
            const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
            return (!enabler || enabler()) &&
                   (id == YieldingEnum::VON_MISES || id == YieldingEnum::DRUCKER_PRAGER);
        });
    category.connect<Float>("Internal friction []", body, BodySettingsId::INTERNAL_FRICTION)
        .setEnabler(enablerDp);
    category.connect<Float>("Cohesion [Pa]", body, BodySettingsId::COHESION).setEnabler(enablerDp);
    category.connect<Float>("Dry friction []", body, BodySettingsId::DRY_FRICTION).setEnabler(enablerDp);
    category.connect<bool>("Use acoustic fludization", body, BodySettingsId::USE_ACOUSTIC_FLUDIZATION)
        .setEnabler(enablerDp);
    category.connect<Float>("Oscillation decay time [s]", body, BodySettingsId::OSCILLATION_DECAY_TIME)
        .setEnabler(enablerAf);
    category.connect<Float>("Fludization viscosity", body, BodySettingsId::FLUIDIZATION_VISCOSITY)
        .setEnabler(enablerAf);
    category.connect<EnumWrapper>("Fragmentation", body, BodySettingsId::RHEOLOGY_DAMAGE).setEnabler(enabler);
}

// ----------------------------------------------------------------------------------------------------------
// MaterialWorker
// ----------------------------------------------------------------------------------------------------------

MaterialWorker::MaterialWorker(const std::string& name, const BodySettings& overrides)
    : IMaterialWorker(name)
    , MaterialProvider(overrides) {}

VirtualSettings MaterialWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& materialCat = connector.addCategory("Material");
    this->addMaterialEntries(materialCat, nullptr);

    VirtualSettings::Category& integratorCat = connector.addCategory("Time step control");
    integratorCat.connect<Float>("Density coeff. [kg/m^3]", body, BodySettingsId::DENSITY_MIN);
    integratorCat.connect<Float>("Energy coeff. [J/kg]", body, BodySettingsId::ENERGY_MIN);
    integratorCat.connect<Float>("Stress coeff. [Pa]", body, BodySettingsId::STRESS_TENSOR_MIN);
    integratorCat.connect<Float>("Damage coeff. []", body, BodySettingsId::DAMAGE_MIN);

    return connector;
}


void MaterialWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = Factory::getMaterial(body);
}

static WorkerRegistrar sRegisterMaterial("material", "materials", [](const std::string& name) {
    return makeAuto<MaterialWorker>(name);
});

// these presets only differ in initial parameters, so it's ok if they have different class names
static WorkerRegistrar sRegisterBasalt("basalt", "materials", [](const std::string& name) {
    return makeAuto<MaterialWorker>(name, getMaterial(MaterialEnum::BASALT)->getParams());
});
static WorkerRegistrar sRegisterIce("ice", "materials", [](const std::string& name) {
    return makeAuto<MaterialWorker>(name, getMaterial(MaterialEnum::ICE)->getParams());
});
static WorkerRegistrar sRegisterOlivine("olivine", "materials", [](const std::string& name) {
    return makeAuto<MaterialWorker>(name, getMaterial(MaterialEnum::OLIVINE)->getParams());
});
static WorkerRegistrar sRegisterIron("iron", "materials", [](const std::string& name) {
    return makeAuto<MaterialWorker>(name, getMaterial(MaterialEnum::IRON)->getParams());
});


// ----------------------------------------------------------------------------------------------------------
// DisableDerivativeCriterionWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings DisableDerivativeCriterionWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void DisableDerivativeCriterionWorker::evaluate(const RunSettings& UNUSED(global),
    IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IMaterial> input = this->getInput<IMaterial>("material");

    // basically should clone the material, needs to be generalized if more complex material setups are used
    result = Factory::getMaterial(input->getParams());
    result->setParam(BodySettingsId::STRESS_TENSOR_MIN, LARGE);
    result->setParam(BodySettingsId::DAMAGE_MIN, LARGE);
}

static WorkerRegistrar sRegisterDisabler("optimize timestepping",
    "optimizer",
    "materials",
    [](const std::string& name) { return makeAuto<DisableDerivativeCriterionWorker>(name); });

NAMESPACE_SPH_END
