#pragma once

/// \file Materials.h
/// \brief SPH-specific implementation of particle materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

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

public:
    /// \brief Creates the material by specifying an equation of state.
    ///
    /// Equation of state must not be nullptr.
    EosMaterial(const BodySettings& body, AutoPtr<IEos>&& eos);

    /// \brief Creates the material.
    ///
    /// Equation of state is constructed from parameters in settings.
    explicit EosMaterial(const BodySettings& body);

    /// Evaluate held equation of state.
    /// \param rho Density of particle in code units.
    /// \param u Specific energy of particle in code units
    /// \returns Computed pressure and sound speed as pair.
    Pair<Float> evaluate(const Float rho, const Float u) const;

    /// Returns the equation of state.
    const IEos& getEos() const;

    virtual void create(Storage& storage, const MaterialInitialContext& context) override;

    virtual void initialize(IScheduler& scheduler, Storage& storage, const IndexSequence sequence) override;

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const IndexSequence UNUSED(sequence)) override {
        // nothing
    }
};

/// \brief Generalization of material with equation of state.
///
/// It holds a rheology implementation that modifies pressure and stress tensor. This is done in \ref
/// initialize function, function \ref finalize then integrates the fragmentation model (if used, of course).
class SolidMaterial : public EosMaterial {
private:
    AutoPtr<IRheology> rheology;

public:
    SolidMaterial(const BodySettings& body, AutoPtr<IEos>&& eos, AutoPtr<IRheology>&& rheology);

    explicit SolidMaterial(const BodySettings& body);

    virtual void create(Storage& storage, const MaterialInitialContext& context) override;

    virtual void initialize(IScheduler& scheduler, Storage& storage, const IndexSequence sequence) override;

    virtual void finalize(IScheduler& scheduler, Storage& storage, const IndexSequence sequence) override;
};

/// \brief Basic materials available in the code.
///
/// Parameters were taken from Reinhardt and Stadel (2016).
enum class MaterialEnum {
    BASALT,
    IRON,
    ICE,
    OLIVINE,
};

AutoPtr<IMaterial> getMaterial(const MaterialEnum type);

NAMESPACE_SPH_END
