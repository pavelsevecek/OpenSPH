#pragma once

/// \file Material.h
/// \brief SPH-specific implementation of particle material
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/AbstractMaterial.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Eos;
    class Rheology;
}

class NullMaterial : public Abstract::Material {
public:
    NullMaterial(const BodySettings& body);

    virtual void create(Storage& UNUSED(storage), const MaterialInitialContext& UNUSED(context)) override {}

    virtual void initialize(Storage& UNUSED(storage), const IndexSequence UNUSED(sequence)) override {}

    virtual void finalize(Storage& UNUSED(storage), const IndexSequence UNUSED(sequence)) override {}
};

/// Material holding equation of state
class EosMaterial : public Abstract::Material {
private:
    AutoPtr<Abstract::Eos> eos;
    ArrayView<Float> rho, u, p, cs;

public:
    EosMaterial(const BodySettings& body, AutoPtr<Abstract::Eos>&& eos);

    /// Evaluate holded equation of state.
    /// \param rho Density of particle in code units.
    /// \param u Specific energy of particle in code units
    /// \returns Computed pressure and sound speed as pair.
    Pair<Float> evaluate(const Float rho, const Float u) const;

    /// Returns the equation of state.
    const Abstract::Eos& getEos() const;

    virtual void create(Storage& storage, const MaterialInitialContext& context) override;

    virtual void initialize(Storage& storage, const IndexSequence sequence) override;

    virtual void finalize(Storage& UNUSED(storage), const IndexSequence UNUSED(sequence)) override {
        // nothing
    }
};

/// Solid material is a generalization of material with equation of state, also having rheology that
/// modifies pressure and stress tensor.
class SolidMaterial : public EosMaterial {
private:
    AutoPtr<Abstract::Rheology> rheology;

public:
    SolidMaterial(const BodySettings& body,
        AutoPtr<Abstract::Eos>&& eos,
        AutoPtr<Abstract::Rheology>&& rheology);

    virtual void create(Storage& storage, const MaterialInitialContext& context) override;

    virtual void initialize(Storage& storage, const IndexSequence sequence) override;

    virtual void finalize(Storage& storage, const IndexSequence sequence) override;
};

/// Returns material using default settings.
AutoPtr<Abstract::Material> getDefaultMaterial();


NAMESPACE_SPH_END
