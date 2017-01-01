#pragma once

#include "geometry/TracelessTensor.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

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
    void update(Storage& storage) {
        y.clear();
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            const Float limit = storage.getMaterial(i).elasticityLimit;
            ASSERT(limit > 0._f);
            y.push(limit);
        }
    }

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        const Float invariant = 1.5_f * ddot(s, s);
        /// \todo sqrt here?
        return s * Math::min(Math::sqr(y[i]) / invariant, 1._f);
    }
};

NAMESPACE_SPH_END
