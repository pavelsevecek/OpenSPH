#include "quantities/Material.h"
#include "physics/Eos.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Material::Material() = default;

Material::~Material() = default;


MaterialAccessor::MaterialAccessor(Storage& storage) {
    materials = storage.getMaterials();
    matIdxs = storage.getValue<Size>(QuantityIds::MATERIAL_IDX);
}

void MaterialAccessor::setParams(const BodySettingsIds paramIdx, const BodySettings& settings) {
    for (Material& mat : materials) {
        settings.copyValueTo(paramIdx, mat.params);
    }
}

EosAccessor::EosAccessor(Storage& storage) {
    materials = storage.getMaterials();
    matIdxs = storage.getValue<Size>(QuantityIds::MATERIAL_IDX);
    tie(rho, u) = storage.getValues<Float>(QuantityIds::DENSITY, QuantityIds::ENERGY);
}

Tuple<Float, Float> EosAccessor::evaluate(const Size particleIdx) const {
    const Material& mat = materials[matIdxs[particleIdx]];
    ASSERT(mat.eos);
    return mat.eos->evaluate(rho[particleIdx], u[particleIdx]);
}

NAMESPACE_SPH_END
