#pragma once

#include "objects/containers/Array.h"
#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

template <typename TValue>
class Volume {
private:
    Array<TValue> data;
    Box box;
    Size res = 0;

public:
    Volume() = default;

    Volume(const Box& box, const Size resolution)
        : box(box)
        , res(resolution) {
        data.resize(pow<3>(res));
        data.fill(TValue(0));
    }

    TValue operator()(const Vector& r) const {
        const Vector idxs = (r - box.lower()) / box.size() * res;
        return data[map(clamp(idxs[X]), clamp(idxs[Y]), clamp(idxs[Z]))];
    }


    TValue& operator()(const Vector& r) {
        const Vector idxs = (r - box.lower()) / box.size() * res;
        return data[map(clamp(idxs[X]), clamp(idxs[Y]), clamp(idxs[Z]))];
    }

    TValue& cell(const Size x, const Size y, const Size z) {
        return data[map(x, y, z)];
    }

    Size size() const {
        return res;
    }

    bool empty() const {
        return res == 0;
    }

private:
    Size clamp(const Float f) const {
        return Sph::clamp(int(f), 0, int(res));
    }

    Size map(const Size x, const Size y, const Size z) const {
        return x + y * res + z * sqr(res);
    }
};

NAMESPACE_SPH_END
