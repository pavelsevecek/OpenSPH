#include "quantities/IMaterial.h"

NAMESPACE_SPH_BEGIN

const Interval IMaterial::DEFAULT_RANGE = Interval::unbounded();
const Float IMaterial::DEFAULT_MINIMAL = 0._f;


void IMaterial::setRange(const QuantityId id, const Interval& range, const Float minimal) {
    auto rangeIter = ranges.find(id);
    if (range == DEFAULT_RANGE) {
        // for unbounded range, we don't have to store the value (unbounded range is default)
        if (rangeIter != ranges.end()) {
            ranges.erase(rangeIter);
        }
    } else {
        ranges[id] = range;
    }

    auto minimalIter = minimals.find(id);
    if (minimal == DEFAULT_MINIMAL) {
        // same thing with minimals - no need to store 0
        if (minimalIter != minimals.end()) {
            minimals.erase(minimalIter);
        }
    } else {
        minimals[id] = minimal;
    }
}


NAMESPACE_SPH_END
