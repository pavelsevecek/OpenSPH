#include "quantities/IMaterial.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

const Interval IMaterial::DEFAULT_RANGE = Interval::unbounded();
const Float IMaterial::DEFAULT_MINIMAL = 0._f;

MaterialInitialContext::MaterialInitialContext(const RunSettings& settings) {
    rng = Factory::getRng(settings);
    scheduler = Factory::getScheduler(settings);
    kernelRadius = Factory::getKernel<3>(settings).radius();
    generateUvws = settings.get<bool>(RunSettingsId::GENERATE_UVWS);
}

void IMaterial::setRange(const QuantityId id, const Interval& range, const Float minimal) {
    if (range == DEFAULT_RANGE) {
        // for unbounded range, we don't have to store the value (unbounded range is default)
        ranges.tryRemove(id);
    } else {
        ranges.insert(id, range);
    }

    if (minimal == DEFAULT_MINIMAL) {
        // same thing with minimals - no need to store 0
        minimals.tryRemove(id);
    } else {
        minimals.insert(id, minimal);
    }
}


NAMESPACE_SPH_END
