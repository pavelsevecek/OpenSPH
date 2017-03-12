#pragma once

#include "quantities/AbstractMaterial.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Eos;
    class Rheology;
}

/// Material holding equation of state
class EosMaterial : public Abstract::Material {
private:
    std::unique_ptr<Abstract::Eos> eos;

public:
    EosMaterial(std::unique_ptr<Abstract::Eos>&& eos)
        : eos(std::move(eos)) {}

    virtual void initialize(Storage& storage, const MaterialSequence sequence) override {
        tie(rho, u) = storage.getValues<Float>(QuantityIds::DENSITY, QuantityIds::ENERGY);
        for (Size matIdx : sequence) {
            /// \todo now we can easily pass sequence into the EoS and iterate inside, to avoid calling
            /// virtual function (and we could also optimize with SSE)
            eos->evaluate(rho[i], u[i]);
        }
    }

    virtual void finalize(Storage& UNUSED(storage)) override {}
};


/// Solid material is a generalization of material with equation of state, also having rheology that modifies
/// pressure and stress tensor.
class SolidMaterial : public EosMaterial {
private:
    std::unique_ptr<Abstract::Rheology> rheology;

public:
    virtual void initialize(Storage& storage) override {
        EosMaterial::initialize(storage);
        rheology->initialize(storage);
    }

    virtual void finalize(Storage& storage) override {
        rheology->integrate(storage);
    }
}


NAMESPACE_SPH_END
