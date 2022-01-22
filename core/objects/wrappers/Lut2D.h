#pragma once

#include "math/MathUtils.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Interval.h"
#include <algorithm>

NAMESPACE_SPH_BEGIN

template <typename TValue>
class Lut2D : public Noncopyable {
private:
    Array<TValue> data;
    Size width;
    Size height;
    Array<Float> valuesX;
    Array<Float> valuesY;

public:
    Lut2D() = default;

    Lut2D(const Size width, const Size height, Array<Float>&& valuesX, Array<Float>&& valuesY)
        : width(width)
        , height(height)
        , valuesX(std::move(valuesX))
        , valuesY(std::move(valuesY)) {
        SPH_ASSERT(std::is_sorted(this->valuesX.begin(), this->valuesX.end()));
        SPH_ASSERT(std::is_sorted(this->valuesY.begin(), this->valuesY.end()));
        data.resize(width * height);
    }

    INLINE TValue& at(const Size x, const Size y) {
        return data[map(x, y)];
    }

    INLINE const TValue& at(const Size x, const Size y) const {
        return data[map(x, y)];
    }

    ArrayView<const TValue> row(const Size y) const {
        return ArrayView<const TValue>(&data[map(0, y)], width);
    }

    const TValue interpolate(const Float x, const Float y) const {
        const Size ix1 = this->findIndex(valuesX, x);
        const Size iy1 = this->findIndex(valuesY, y);
        const Float x1 = valuesX[ix1];
        Float dx;
        Size ix2;
        if (ix1 < valuesX.size() - 1) {
            ix2 = ix1 + 1;
            const Float x2 = valuesX[ix2];
            dx = x2 != x1 ? (x - x1) / (x2 - x1) : 0;
        } else {
            /// \todo extrapolate?
            ix2 = ix1;
            dx = 0;
        }
        const Float y1 = valuesY[iy1];
        Float dy;
        Size iy2;
        if (iy1 < valuesY.size() - 1) {
            iy2 = iy1 + 1;
            const Float y2 = valuesY[iy2];
            dy = y2 != y1 ? (y - y1) / (y2 - y1) : 0;
        } else {
            iy2 = iy1;
            dy = 0;
        }
        const TValue v11 = this->at(ix1, iy1);
        const TValue v12 = this->at(ix1, iy2);
        const TValue v21 = this->at(ix2, iy1);
        const TValue v22 = this->at(ix2, iy2);
        return v11 * (1 - dx) * (1 - dy) + v12 * (1 - dx) * dy + v21 * dx * (1 - dy) + v22 * dx * dy;
    }

private:
    INLINE Size findIndex(ArrayView<const Float> values, const Float x) const {
        auto iter = std::upper_bound(values.begin(), values.end(), x);
        if (iter == values.begin()) {
            return 0;
        } else if (iter == values.end()) {
            return values.size() - 1;
        } else {
            const Size idx = iter - values.begin();
            SPH_ASSERT(idx > 0);
            return idx - 1;
        }
    }

    INLINE Size map(const Size x, const Size y) const {
        return y * width + x;
    }
};

NAMESPACE_SPH_END
