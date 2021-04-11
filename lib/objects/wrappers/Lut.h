#pragma once

/// \file Lut.h
/// \brief Approximation of generic function by look-up table
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Array.h"
#include "objects/wrappers/Interval.h"

NAMESPACE_SPH_BEGIN

/// \brief Callable representing a generic R->T function, approximated using look-up table.
template <typename TValue, typename TScalar = Float>
class Lut {
private:
    Array<TValue> data;
    Interval range;

public:
    Lut(const Interval range, Array<TValue>&& data)
        : data(std::move(data))
        , range(range) {}

    template <typename TFunction>
    Lut(const Interval range, const Size resolution, TFunction&& func)
        : data(resolution)
        , range(range) {
        for (Size i = 0; i < resolution; ++i) {
            const Float x = range.lower() + Float(i) * range.size() / (resolution - 1);
            data[i] = func(x);
        }
    }

    INLINE TValue operator()(const TScalar x) const {
        const TScalar fidx = TScalar((x - range.lower()) / range.size() * (data.size() - 1));
        const int idx1 = int(fidx);
        const int idx2 = idx1 + 1;
        if (idx2 >= int(data.size() - 1)) {
            /// \todo possibly make linear interpolation rather than just const value
            return data.back();
        } else if (idx1 <= 0) {
            return data.front();
        } else {
            const TScalar ratio = fidx - idx1;
            SPH_ASSERT(ratio >= TScalar(0) && ratio < TScalar(1));
            return lerp(data[idx1], data[idx2], ratio);
        }
    }

    Interval getRange() const {
        return range;
    }
};

NAMESPACE_SPH_END
