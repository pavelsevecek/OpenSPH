#pragma once

#include "geometry/TracelessTensor.h"
#include "objects/ForwardDecl.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Storage;

class DummyYielding {
public:
    void initialize(Storage& storage, const BodySettings& settings) const;

    void update(Storage& UNUSED(storage)) {}
};

/// \todo this is hardcoded for scalar damage, generalize (merge together yielding and fragmentation)
class VonMises {
private:
public:
    void initialize(Storage& storage, const BodySettings& settings) const;

    void update(Storage& storage);
};


/// Collins et al. (2004)
class DruckerPrager {
private:
    /// \todo Fix implementation according to von Mises

    Array<Float> yieldingStress;

public:
    void update(Storage& storage);

    /// \todo code duplication
    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        ASSERT(yieldingStress[i] > EPS);
        const Float inv = 0.5_f * ddot(s, s) / sqr(yieldingStress[i]);
        if (inv < EPS) {
            return s;
        } else {
            ASSERT(isReal(inv));
            const TracelessTensor s_red = s * min(sqrt(1._f / (3._f * inv)), 1._f);
            ASSERT(isReal(s_red));
            return s_red;
        }
    }
};

NAMESPACE_SPH_END
