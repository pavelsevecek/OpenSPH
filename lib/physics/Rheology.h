#pragma once

/// \file Rheology.h
/// \brief Rheology of materials
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Damage;

    /// \brief Base class of rheological models
    ///
    /// Shall be only used in \ref SolidMaterial, functions do not have to be called directly from the solver.
    class Rheology : public Polymorphic {
    public:
        /// Creates all the necessary quantities and material parameters needed by the rheology. The function
        /// is called by each body added to the simulation.
        /// \param storage Particle storage, containing particle positions and their masses (optionally also
        ///                other quantities). Particles belong only to the body being created, other bodies
        ///                have separate storages.
        /// \param settings Parameters of the body being created.
        virtual void create(Storage& storage, const BodySettings& settings) const = 0;

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
}

/// Introduces plastic behavior for stress tensor, using von Mises yield criterion \cite vonMises_1913.
class VonMisesRheology : public Abstract::Rheology {
private:
    std::unique_ptr<Abstract::Damage> damage;

public:
    /// Constructs a rheology with no fragmentation model. Stress tensor is only modified by von Mises
    /// criterion, yielding strength does not depend on damage.
    VonMisesRheology();

    /// Constructs a rheology with given fragmentation model.
    VonMisesRheology(std::unique_ptr<Abstract::Damage>&& damage);

    ~VonMisesRheology();

    virtual void create(Storage& storage, const BodySettings& settings) const override;

    virtual void initialize(Storage& storage, const MaterialView material) override;

    virtual void integrate(Storage& storage, const MaterialView material) override;
};


/// Pressure dependent failure modes \cite Collins_2004
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
