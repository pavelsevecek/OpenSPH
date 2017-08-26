#pragma once

/// \file Rheology.h
/// \brief Rheology of materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/geometry/TracelessTensor.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

struct MaterialInitialContext;
class IDamage;

/// \brief Base class of rheological models
///
/// Shall be only used in \ref SolidMaterial, functions do not have to be called directly from the solver.
class IRheology : public Polymorphic {
public:
    /// Creates all the necessary quantities and material parameters needed by the rheology. The function
    /// is called by each body added to the simulation.
    /// \param storage Particle storage, containing particle positions and their masses (optionally also
    ///                other quantities). Particles belong only to the body being created, other bodies
    ///                have separate storages.
    /// \param material Material containing input material parameters. The rheology may sets the
    ///                 timestepping parameters (range and minimal values) of the material.
    /// \param context Shared data for creating all materials in the simulation.
    virtual void create(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const = 0;

    /// Evaluates the stress tensor reduction factors. Called for every material in the simulation, before
    /// iteration over particle pairs
    /// \param storage Storage including all the particles.
    /// \param material Material properties and sequence of particles with this material. Implementation
    ///                 should only modify particles with indices in this sequence.
    virtual void initialize(Storage& storage, const MaterialView material) = 0;

    /// Computes derivatives of the time-dependent quantities of the rheological model. Called for every
    /// material in the simulation, after all derivatives are computed.
    /// \param storage Storage including all the particles.
    /// \param material Material properties and sequence of particles with this material. Implementation
    ///                 should only modify particles with indices in this sequence.
    virtual void integrate(Storage& storage, const MaterialView material) = 0;
};


/// Introduces plastic behavior for stress tensor, using von Mises yield criterion \cite vonMises_1913.
class VonMisesRheology : public IRheology {
private:
    AutoPtr<IDamage> damage;

public:
    /// Constructs a rheology with no fragmentation model. Stress tensor is only modified by von Mises
    /// criterion, yielding strength does not depend on damage.
    VonMisesRheology();

    /// Constructs a rheology with given fragmentation model.
    VonMisesRheology(AutoPtr<IDamage>&& damage);

    ~VonMisesRheology();

    virtual void create(Storage& storage,
        IMaterial& settings,
        const MaterialInitialContext& context) const override;

    virtual void initialize(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};


/// Pressure dependent failure modes \cite Collins_2004
class DruckerPragerRheology : public IRheology {
private:
    AutoPtr<IDamage> damage;

    /// \todo Fix implementation according to von Mises

    Array<Float> yieldingStress;

public:
    DruckerPragerRheology(AutoPtr<IDamage>&& damage);

    ~DruckerPragerRheology();

    virtual void create(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

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

/// Perfectly elastic material, no yielding nor fragmentation
class ElasticRheology : public IRheology {
public:
    virtual void create(Storage& storage,
        IMaterial& material,
        const MaterialInitialContext& context) const override;

    virtual void initialize(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};

NAMESPACE_SPH_END
