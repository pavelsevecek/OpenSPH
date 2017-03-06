#include "quantities/AbstractMaterial.h"
#include "physics/Eos.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


MaterialAccessor::MaterialAccessor(Storage& storage) {
    materials = storage.getMaterials();
    matIdxs = storage.getValue<Size>(QuantityIds::MATERIAL_IDX);
}

void MaterialAccessor::setParams(const BodySettingsIds paramIdx, const BodySettings& settings) {
    for (Material& mat : materials) {
        settings.copyValueTo(paramIdx, mat.params);
    }
}


NAMESPACE_SPH_END
