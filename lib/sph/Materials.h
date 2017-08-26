#pragma once

/// \file Materials.h
/// \brief SPH-specific implementation of particle materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/IMaterial.h"

NAMESPACE_SPH_BEGIN

class IEos;
class IRheology;

class NullMaterial : public IMaterial {
public:
    NullMaterial(const BodySettings& body);

    virtual void create(Storage& UNUSED(storage), const MaterialInitialContext& UNUSED(context)) override {}

    virtual void initialize(Storage& UNUSED(storage), const IndexSequence UNUSED(sequence)) override {}

    virtual void finalize(Storage& UNUSED(storage), const IndexSequence UNUSED(sequence)) override {}
};

/// Material holding equation of state
class EosMaterial : public IMaterial {
private:
    AutoPtr<IEos> eos;
    ArrayView<Float> rho, u, p, cs;

public:
    EosMaterial(const BodySettings& body, AutoPtr<IEos>&& eos);

    /// Evaluate holded equation of state.
    /// \param rho Density of particle in code units.
    /// \param u Specific energy of particle in code units
    /// \returns Computed pressure and sound speed as pair.
    Pair<Float> evaluate(const Float rho, const Float u) const;

    /// Returns the equation of state.
    const IEos& getEos() const;

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
    AutoPtr<IRheology> rheology;

public:
    SolidMaterial(const BodySettings& body, AutoPtr<IEos>&& eos, AutoPtr<IRheology>&& rheology);

    virtual void create(Storage& storage, const MaterialInitialContext& context) override;

    virtual void initialize(Storage& storage, const IndexSequence sequence) override;

    virtual void finalize(Storage& storage, const IndexSequence sequence) override;
};

/// Returns material using default settings.
AutoPtr<IMaterial> getDefaultMaterial();


NAMESPACE_SPH_END
