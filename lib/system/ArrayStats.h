#pragma once

#include "system/Logger.h"
#include "objects/containers/ArrayView.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

template <typename Type>
class ArrayStats {
private:
    Type minValue = std::numeric_limits<Type>::max();
    Type maxValue = std::numeric_limits<Type>::min();
    Type medianValue = Type(0);
    double averageValue = 0.;

public:
    /// \todo flags for each stat
    ArrayStats(ArrayView<const Type> values) {
        for (const Type& v : values) {
            minValue = Sph::min(minValue, v);
            maxValue = Sph::max(maxValue, v);
            averageValue += v;
        }
        if (values.empty()) {
            return;
        }
        averageValue /= values.size();
        const Size mid = values.size() >> 1;
        Array<Float> cloned;
        cloned.pushAll(values.begin(), values.end());
        std::nth_element(cloned.begin(), cloned.begin() + mid, cloned.end());
        medianValue = cloned[mid];
    }

    INLINE Type min() const { return minValue; }

    INLINE Type max() const { return maxValue; }

    INLINE Range range() const { return Range(Float(minValue), Float(maxValue)); }

    INLINE Type average() const { return averageValue; }

    INLINE Type median() const { return medianValue; }

    void write(Abstract::Logger& logger) {
        logger.write("Min = " + std::to_string(min()) + "; Max = " + std::to_string(max()) + "; Mean = " +
                     std::to_string(average()) + "; Median = " + std::to_string(median()));
    }
};


NAMESPACE_SPH_END
