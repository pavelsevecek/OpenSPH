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
};


NAMESPACE_SPH_END
