#pragma once

#include "objects/containers/ArrayView.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN


/// Class computing integrals of motion, average/minimal/maximal values of quantities, etc.
/// For non-inertial reference frame, all relevant values are evaluated in the inertial reference frame,
/// meaning even for zero particle velocities (in the non-inertial frame of the run), the resulting momentum
/// or angular momentum can be nonzero.
/// \todo automatically exclude ghost particles?
class Integrals {
private:
    Vector omega{ 0._f };

public:
    /// Default construction assumes inertial reference frame.
    Integrals();

    Integrals(const GlobalSettings& settings);

    /// Computes the total mass of all SPH particles. Storage must contains particle masses, of course;
    /// checked by assert.
    /// \note Total mass is always conserved automatically as particles do not change their mass. This is
    /// therefore only useful as a sanity check, or potentially if a solver with variable particle masses gets
    /// implemented.
    Float getTotalMass(Storage& storage) const;

    /// Computes total momentum of all SPH particles with a respect to the center of reference frame.
    /// Storage must contain at least particle masses and particle positions with velocities, checked by
    /// assert.
    Vector getTotalMomentum(Storage& storage) const;

    /// Computes total angular momentum of all SPH particles with a respect to the center of reference frame.
    /// Storage must contain at least particle masses and particle positions with velocities, checked by
    /// assert.
    Vector getTotalAngularMomentum(Storage& storage) const;

    /// Returns the total energy of all particles. This is simply of sum of total kinetic energy and total
    /// internal energy.
    /// \todo this has to be generalized if some external potential is used.
    Float getTotalEnergy(Storage& storage) const;

    /// Returns the total kinetic energy of all particles. Storage must contain at least particle masses and
    /// particle positions with velocities, checked by assert.
    Float getTotalKineticEnergy(Storage& storage) const;

    /// Returns the total internal energy of all particles. Storage must contain at least particle masses and
    /// specific internal energy. If used solver works with other independent quantity (energy density, total
    /// energy, specific entropy), specific energy must be derived before the function is called.
    Float getTotalInternalEnergy(Storage& storage) const;

    /// Computes the center of mass of all particles, or optionally center of mass of particles
    /// belonging to body of given ID. The center is evaluated with a respect to reference frame.
    Vector getCenterOfMass(Storage& storage, const Optional<Size> bodyId = NOTHING) const;
};

/// Class computing other non-physical statistics of SPH simulations.
/// \note These functions generally take longer to compute, they are not part of the solver to avoid
/// unnecessary overhead.
/// \todo move to separate file
/// \todo better name
class Diagnostics {
private:
    GlobalSettings settings;

public:
    Diagnostics(const GlobalSettings& settings);

    /// Returns the number of separate bodies SPH particles form. Two bodies are considered separate if each
    /// pair of particles from different bodies is further than h * kernel.radius.
    /// \todo this should account for symmetrized smoothing lenghts.
    Size getNumberOfComponents(Storage& storage) const;

    struct Pair {
        Size i1, i2;
    };
    /// Returns the list of particles forming pairs, i.e. particles on top of each other or very close. If
    /// the array is not empty, this is a sign of pairing instability or multi-valued velocity field, both
    /// unwanted artefacts in SPH simulations.
    /// This might occur because of numerical instability, possibly due to time step being too high, or due to
    /// division by very small number in evolution equations. If the pairing instability occurs regardless,
    /// try choosing different parameter SPH_KERNEL_ETA (should be aroung 1.5), or by choosing different SPH
    /// kernel.
    /// \param limit Maximal distance of two particles forming a pair in units of smoothing length.
    /// \returns Detected pairs of particles given by their indices in the array, in no particular order.
    Array<Pair> getParticlePairs(Storage& storage, const Float limit = 1.e-2_f) const;
};


NAMESPACE_SPH_END
