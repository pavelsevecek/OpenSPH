#pragma once

#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Storage;


namespace Abstract {
    class Damage;

    class Rheology : public Polymorphic {
    public:
        virtual void create(Storage& storage, const BodySettings& settings) = 0;

        virtual void initialize(Storage& storage, const MaterialSequence sequence) = 0;

        /// Computes derivatives of the time-dependent quantities of the rheological model.
        virtual void integrate(Storage& storage, const MaterialSequence sequence) = 0;
    };
}


class VonMisesRheology : public Abstract::Rheology {
private:
    std::unique_ptr<Abstract::Damage> damage;

public:
    virtual void create(Storage& storage, const BodySettings& settings) override;

    virtual void initialize(Storage& storage, const MaterialSequence sequence) override;

    virtual void integrate(Storage& storage, const MaterialSequence sequence) override;
};


/// Collins et al. (2004)
class DruckerPragerRheology : public Abstract::Rheology {
private:
    std::unique_ptr<Abstract::Damage> damage;

    /// \todo Fix implementation according to von Mises

    Array<Float> yieldingStress;

public:
    virtual void initialize(Storage& storage, const MaterialSequence sequence) override;

    virtual void integrate(Storage& storage, const MaterialSequence sequence) override;

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
