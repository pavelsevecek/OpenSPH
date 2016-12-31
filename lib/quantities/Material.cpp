#include "quantities/Material.h"
#include "physics/Eos.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Material::Material() = default;

Material::~Material() = default;

Material::Material(const BodySettings& settings) {
    eos = Factory::getEos(settings);
    shearModulus = settings.get<Float>(BodySettingsIds::SHEAR_MODULUS);
    elasticityLimit = settings.get<Float>(BodySettingsIds::ELASTICITY_LIMIT);
    adiabaticIndex = settings.get<Float>(BodySettingsIds::ADIABATIC_INDEX);
}

Material::Material(Material&& other)
    : eos(std::move(other.eos))
    , shearModulus(other.shearModulus)
    , elasticityLimit(other.elasticityLimit)
    , adiabaticIndex(other.adiabaticIndex) {}

Material& Material::operator=(Material&& other) {
    eos = std::move(other.eos);
    shearModulus = other.shearModulus;
    elasticityLimit = other.elasticityLimit;
    adiabaticIndex = other.adiabaticIndex;
    return *this;
}

NAMESPACE_SPH_END
