#pragma once

#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Storage;

class DummyYielding  {
public:
    void update(Storage& UNUSED(storage)) {}

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int UNUSED(i)) const { return s; }
};


class VonMisesYielding  {
private:
    // cached values of elasticity limit
    Array<Float> y;

public:
    void update(Storage& storage);

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        const Float invariant = 1.5_f * ddot(s, s);
        /// \todo sqrt here?
        return s * Math::min(Math::sqr(y[i]) / invariant, 1._f);
    }
};

NAMESPACE_SPH_END
