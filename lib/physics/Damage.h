#pragma once

#include "common/ForwardDecl.h"
#include "geometry/TracelessTensor.h"
#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Damage : public Polymorphic {
    public:
        /// Sets up all the necessary quantities in the storage given material settings.
        virtual void setFlaws(Storage& storage, const BodySettings& settings) const = 0;

        /// Computes modified values of pressure and stress tensor due to fragmentation.
        virtual void reduce(Storage& storage, const MaterialView sequence) = 0;

        /// Compute damage derivatives
        virtual void integrate(Storage& storage, const MaterialView sequence) = 0;
    };
}

enum class ExplicitFlaws {
    UNIFORM,  ///< Distribute flaws uniformly (to random particles), see Benz & Asphaug (1994), Sec. 3.3.1
    ASSIGNED, ///< Explicitly assigned flaws by user, used mainly for testing purposes. Values must be set in
              /// quantity
};

/// Scalar damage describing fragmentation of the body according to Grady-Kipp model (Grady and Kipp, 1980)
class ScalarDamage : public Abstract::Damage {
private:
    Float kernelRadius;

    ExplicitFlaws options;

public:
    ScalarDamage(const Float kernelRadius, const ExplicitFlaws options = ExplicitFlaws::UNIFORM);

    virtual void setFlaws(Storage& storage, const BodySettings& settings) const override;

    virtual void reduce(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};

class TensorDamage : public Abstract::Damage {
private:
public:
    virtual void setFlaws(Storage& storage, const BodySettings& settings) const override;

    virtual void reduce(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};

class NullDamage : public Abstract::Damage {
public:
    virtual void setFlaws(Storage& storage, const BodySettings& settings) const override;

    virtual void reduce(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};

NAMESPACE_SPH_END
