#pragma once

#include "quantities/AbstractMaterial.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Eos;
    class Rheology;
}

class NullMaterial : public Abstract::Material {
public:
    virtual void initialize(Storage& UNUSED(storage), const MaterialSequence UNUSED(sequence)) override {}

    virtual void finalize(Storage& UNUSED(storage), const MaterialSequence UNUSED(sequence)) override {}
};

/// Material holding equation of state
class EosMaterial : public Abstract::Material {
private:
    std::unique_ptr<Abstract::Eos> eos;
    ArrayView<Float> rho, u, p, cs;

public:
    EosMaterial(std::unique_ptr<Abstract::Eos>&& eos);

    virtual void initialize(Storage& storage, const MaterialSequence sequence) override;

    virtual void finalize(Storage& UNUSED(storage), const MaterialSequence UNUSED(sequence)) override {
        // nothing
    }
};

/// Solid material is a generalization of material with equation of state, also having rheology that
/// modifies
/// pressure and stress tensor.
class SolidMaterial : public EosMaterial {
private:
    std::unique_ptr<Abstract::Rheology> rheology;

public:
    SolidMaterial(std::unique_ptr<Abstract::Eos>&& eos, std::unique_ptr<Abstract::Rheology>&& rheology);

    virtual void initialize(Storage& storage, const MaterialSequence sequence) override;

    virtual void finalize(Storage& storage, const MaterialSequence sequence) override;
};

/// Returns material using default settings.
std::unique_ptr<Abstract::Material> getDefaultMaterial();


NAMESPACE_SPH_END
