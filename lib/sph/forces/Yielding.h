#pragma once

#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Storage;

class DummyYielding {
public:
    void update(Storage& UNUSED(storage)) {}

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int UNUSED(i)) const { return s; }
};


class VonMisesYielding {
private:
    // cached values of elasticity limit
    Array<Float> y;

public:
    void update(Storage& storage);

    /// \param s Deviatoric stress tensor of i-th particle, reduced by fragmentation model if applied.
    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        ASSERT(y[i] > EPS);
        const Float inv = 0.5_f * ddot(s, s) / sqr(y[i]);
        if (inv < EPS) {
            return s;
        } else {
            /// \todo Y depends on temperature, resp. melting energy!
            ASSERT(isReal(inv));
            const TracelessTensor s_red = s * min(sqrt(1._f / (3._f * inv)), 1._f);
            ASSERT(isReal(s_red));
            return s_red;
        }
    }
};

NAMESPACE_SPH_END
