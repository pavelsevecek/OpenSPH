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


template<typename TEnum>
class Settings;
enum class BodySettingsIds;
using BodySettings = Settings<BodySettingsIds>;

class VonMises {
private:
    // cached values of elasticity limit
    Array<Float> yieldingStress;

public:
    void initialize(Storage& storage, const BodySettings& settings) const;

    void update(Storage& storage);

    /// \param s Deviatoric stress tensor of i-th particle, reduced by fragmentation model if applied.
    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        ASSERT(yieldingStress[i] >= 0);
        if (yieldingStress[i] < EPS) {
            return TracelessTensor::null();
        }
        const Float inv = 0.5_f * ddot(s, s) / sqr(yieldingStress[i]) + EPS;
            ASSERT(isReal(inv) && inv > 0._f);
            const TracelessTensor s_red = s * min(sqrt(1._f / (3._f * inv)), 1._f);
            ASSERT(isReal(s_red));
            return s_red;
    }
};


/// Collins et al. (2004)
class DruckerPrager {
private:
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
