#include "storage/Material.h"
#include "physics/Eos.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Material::Material() = default;

Material::~Material() = default;

Material::Material(const BodySettings& settings) {
    eos = Factory::getEos(settings);
    shearModulus = settings.get<Float>(BodySettingsIds::SHEAR_MODULUS);
    elasticityLimit = settings.get<Float>(BodySettingsIds::VON_MISES_ELASTICITY_LIMIT);
}

Material::Material(Material&& other)
    : eos(std::move(other.eos))
    , shearModulus(other.shearModulus)
    , elasticityLimit(other.elasticityLimit) {}

Material& Material::operator=(Material&& other) {
    eos = std::move(other.eos);
    shearModulus = other.shearModulus;
    elasticityLimit = other.elasticityLimit;
    return *this;
}

NAMESPACE_SPH_END
