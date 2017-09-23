#pragma once

/// \file Boundary.h
/// \brief Boundary conditions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "objects/containers/Array.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/Interval.h"
#include "sph/equations/EquationTerm.h"

#include <set>

NAMESPACE_SPH_BEGIN

/// \todo boundary conditions CANNOT be just equation terms, they affect computed derivatives (frozen
/// particles), or added new particles we don't want to touch by other equations.
/// They must either be equation terms evaluated LAST, or be a separate object


/// \brief Base object for boundary conditions.
///
/// All boundary conditions are essentially equation terms, but they do not create any quantities nor they
/// compute derivatives.
class IBoundaryCondition : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder&, const RunSettings&) override {}

    virtual void create(Storage&, IMaterial&) const override {}
};


/// \brief Adds ghost particles symmetrically for each SPH particle close to boundary.
///
/// All physical quantities are copied on them. This acts as a natural boundary for SPH particles without
/// creating unphysical gradients due to discontinuity.
class GhostParticles : public IBoundaryCondition {
private:
    AutoPtr<IDomain> domain;
    // index where the ghost particles begin (they are always stored successively)
    Array<Ghost> ghosts;
    Array<Size> ghostIdxs; // indices of ghost particles in the storage
    Array<Float> distances;
    Float searchRadius;
    Float minimalDist;

public:
    GhostParticles(AutoPtr<IDomain>&& domain, const RunSettings& settings);

    /// \todo we hold indices into the storage we get from parameter. There is no guarantee the storage is the
    /// same each time and that it wasn't changed. Think of a better way of doing this, possibly by creating
    /// helper quantity (index of source particle for ghosts and something like -1 for regular particles.
    virtual void initialize(Storage& storage) override;

    virtual void finalize(Storage& UNUSED(storage)) override {}

    /// \brief Removes all ghost particles from the storage.
    ///
    /// This does not have to be called before apply, ghosts are removed and added automatically.
    /// \todo Currently needs to be called before data output as ghosts would be saved together with "regular"
    ///       particles. This shouldn't happen, ghosts should be removed automatically.
    void removeGhosts(Storage& storage);
};


/// \brief Boundary condition that nulls all highest derivates of selected particles.
///
/// Term can freeze particles close to the boundary and/or particles of given body or list of bodies. This
/// will cause the particles to keep quantity values given by their initial conditions and move with initial
/// velocity. The 'frozen' particles affect other particles normally and contribute to all integrals, such as
/// total mometum or energy.
class FrozenParticles : public IBoundaryCondition {
protected:
    AutoPtr<IDomain> domain;
    Float radius;

    std::set<Size> frozen;

    Array<Float> distances;
    Array<Size> idxs;

public:
    /// \brief Constructs boundary conditions with no particles frozen.
    ///
    /// These can be later added using freeze function.
    FrozenParticles();

    ~FrozenParticles();

    /// \brief Constructs boundary conditions with frozen particles near boundary.
    ///
    /// \param domain Domain defining the boundary
    /// \param radius Search radius (in units of smoothing length) up to which the particles will be frozen.
    FrozenParticles(AutoPtr<IDomain>&& domain, const Float radius);

    /// \brief Adds body ID particles of which shall be frozen by boundary conditions.
    void freeze(const Size flag);

    /// \brief Remove a body from the list of frozen bodies.
    ///
    /// If the body is not on the list, nothing happens.
    void thaw(const Size flag);

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override;
};


/// \brief Boundary conditions creating particles with given velocity at the boundary of the domain.
///
/// Outgoing particles are removed at the other side of the boundary, particles inside the domain are kept in
/// place using either ghost particles or by freezing them. This is fine-tuned for simulations of a meteorite
/// passing through athmosphere.
class WindTunnel : public FrozenParticles {
public:
    WindTunnel(AutoPtr<IDomain>&& domain, const Float radius);

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override;
};

/// Boundary condition moving all particles passed through the domain to the other side of the domain.
class PeriodicDomain : public IBoundaryCondition {
    /// \todo modify Finder to search periodically in domain. That should be the whole trick
};


/// Helper tool for 1D tests, projects all particles onto a 1D line.
class Projection1D : public IBoundaryCondition {
private:
    Interval domain;
    ArrayView<Vector> r, v;

public:
    /// Constructs using range as 1D domain
    Projection1D(const Interval& domain);

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override;
};


NAMESPACE_SPH_END
