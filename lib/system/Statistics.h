#pragma once

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

class Statistics : public Object {
private:
    Float minValue = INFTY, maxValue = -INFTY, medianValue = 0._f;
    double averageValue = 0.;

public:
    /// \todo flags for each stat
    Statistics(ArrayView<const Float> values) {
        for (Float v : values) {
            minValue = Math::min(minValue, v);
            maxValue = Math::max(maxValue, v);
            averageValue += v;
        }
        if (values.empty()) {
            return;
        }
        averageValue /= values.size();
        const int mid = values.size() >> 1;
        Array<Float> cloned;
        cloned.pushAll(values.begin(), values.end());
        std::nth_element(cloned.begin(), cloned.begin() + mid, cloned.end());
        medianValue = cloned[mid];
    }

    INLINE Float min() const { return minValue; }

    INLINE Float max() const { return maxValue; }

    INLINE Range range() const { return Range(minValue, maxValue); }

    INLINE Float average() const { return averageValue; }

    INLINE Float median() const { return medianValue; }

    void write(Abstract::Logger& logger) {
        logger.write("Min = " + std::to_string(min()) + "; Max = " + std::to_string(max()) + "; Mean = " +
                     std::to_string(average()) + "; Median = " + std::to_string(median()));
    }
};


NAMESPACE_SPH_END
