#pragma once

/// \file Materials.h
/// \brief SPH-specific implementation of particle materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/IMaterial.h"

NAMESPACE_SPH_BEGIN

class IEos;
class IRheology;

/// \brief Material holding equation of state
///
/// Pressure and sound speed are computed in \ref initialize function, so the EoS does not have to be
/// evaluated manually. If this is necessary for some reason (when setting pressure-dependent initial
/// conditions or checking if we selected correct EoS, for example), functions \ref evaluate and \ref getEos
/// can be used. This is not part of the IMaterial interface, so dynamic_cast have to be used to access it.
class EosMaterial : public IMaterial {
private:
    AutoPtr<IEos> eos;
    ArrayView<Float> rho, u, p, cs;

public:
    /// \brief Creates the material by specifying an equation of state.
    ///
    /// Equation of state must not be nullptr.
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

/// \brief Solid material is a generalization of material with equation of state.
///
/// It holds a rheology implementation that modifies pressure and stress tensor. This is done in \ref
/// initialize function, function \ref finalize then integrates the fragmentation model (if used, of course).
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
