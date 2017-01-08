#include "sph/forces/Yielding.h"
#include "quantities/Storage.h"
#include "quantities/Material.h"

NAMESPACE_SPH_BEGIN

void VonMisesYielding::update(Storage& storage) {
    y.clear();
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        const Float limit = storage.getMaterial(i).elasticityLimit;
        ASSERT(limit > 0._f);
        y.push(limit);
    }
}

NAMESPACE_SPH_END
