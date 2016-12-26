#pragma once

#include "geometry/TracelessTensor.h"
#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN

class DummyYielding : public Object {
public:
    void update(Storage& UNUSED(storage)) {}

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int UNUSED(i)) const { return s; }
};


class VonMisesYielding : public Object {
private:
    // cached values of elasticity limit
    Array<Float> y;

public:
    void update(Storage& storage) {
        y.clear();
        for (int i = 0; i < storage.getParticleCnt(); ++i) {
            y.push(storage.getMaterial(i).elasticityLimit);
        }
    }

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        const Float invariant = 1.5_f * ddot(s, s);
        /// \todo sqrt here?
        return s * Math::min(Math::sqr(y[i]) / invariant, 1._f);
    }
};

NAMESPACE_SPH_END
