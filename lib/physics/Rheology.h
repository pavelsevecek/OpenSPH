#pragma once

#include "common/ForwardDecl.h"
#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Damage;

    class Rheology : public Polymorphic {
    public:
        /// Creates all the necessary quantities and material parameters needed by the rheology.
        virtual void create(Storage& storage, const BodySettings& settings) const = 0;

        /// Evaluates the stress tensor reduction factors. Called before iteration over particle pairs.
        virtual void initialize(Storage& storage, const MaterialView sequence) = 0;

        /// Computes derivatives of the time-dependent quantities of the rheological model.
        virtual void integrate(Storage& storage, const MaterialView sequence) = 0;
    };
}


class VonMisesRheology : public Abstract::Rheology {
private:
    std::unique_ptr<Abstract::Damage> damage;

public:
    /// Constructs a rheology with no fragmentation model. Stress tensor is only modified by von Mises
    /// criterion, yielding strength does not depend on damage.
    VonMisesRheology();

    /// Constructs a rheology with given fragmentation model
    VonMisesRheology(std::unique_ptr<Abstract::Damage>&& damage);

    ~VonMisesRheology();

    virtual void create(Storage& storage, const BodySettings& settings) const override;

    virtual void initialize(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};


/// Collins et al. (2004)
class DruckerPragerRheology : public Abstract::Rheology {
private:
    std::unique_ptr<Abstract::Damage> damage;

    /// \todo Fix implementation according to von Mises

    Array<Float> yieldingStress;

public:
    DruckerPragerRheology(std::unique_ptr<Abstract::Damage>&& damage);

    ~DruckerPragerRheology();

    virtual void create(Storage& storage, const BodySettings& settings) const override;

    virtual void initialize(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;

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
