#pragma once

/// Boundary conditions
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "common/ForwardDecl.h"
#include "geometry/Vector.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Range.h"
#include "sph/equations/EquationTerm.h"
#include <memory>
#include <set>

NAMESPACE_SPH_BEGIN

/// \todo boundary conditions CANNOT be just equation terms, they affect computed derivatives (frozen
/// particles), or added new particles we don't want to touch by other equations.
/// They must either be equation terms evaluated LAST, or be a separate object

namespace Abstract {
    /// Base object for boundary conditions; all boundary conditions are essentially equation terms, but they
    /// do not create any quantities nor they compute derivatives.
    class BoundaryConditions : public EquationTerm {
    public:
        virtual void setDerivatives(DerivativeHolder&, const RunSettings&) override {}

        virtual void create(Storage&, Abstract::Material&) const override {}
    };
}


/// Adds ghost particles symmetrically for each SPH particle close to boundary. All physical quantities are
/// copied on them. This acts as a natural boundary for SPH particles without creating unphysical gradients
/// due to discontinuity.
class GhostParticles : public Abstract::BoundaryConditions {
private:
    std::unique_ptr<Abstract::Domain> domain;
    // index where the ghost particles begin (they are always stored successively)
    Array<Ghost> ghosts;
    Array<Size> ghostIdxs; // indices of ghost particles in the storage
    Array<Float> distances;
    Float searchRadius;
    Float minimalDist;

public:
    GhostParticles(std::unique_ptr<Abstract::Domain>&& domain, const RunSettings& settings);

    /// \todo we hold indices into the storage we get from parameter. There is no guarantee the storage is the
    /// same each time and that it wasn't changed. Think of a better way of doing this, possibly by creating
    /// helper quantity (index of source particle for ghosts and something like -1 for regular particles.
    virtual void initialize(Storage& storage) override;

    virtual void finalize(Storage& UNUSED(storage)) override {}

    /// Removes all ghost particles from the storage. This does not have to be called before apply, ghosts are
    /// removed and added automatically.
    /// \todo Currently needs to be called before data output as ghosts would be saved together with "regular"
    ///       particles. This shouldn't happen, ghosts should be removed automatically.
    void removeGhosts(Storage& storage);
};


/// Boundary condition that nulls all highest derivates of particles close to the boundary and/or particles of
/// given body or list of bodies. This will cause the particles to keep quantity values given by their initial
/// conditions and move with initial velocity. The 'frozen' particles affect other particles normally and
/// contribute to all integrals, such as total mometum or energy.
class FrozenParticles : public Abstract::BoundaryConditions {
protected:
    std::unique_ptr<Abstract::Domain> domain;
    Float radius;

    std::set<Size> frozen;

    Array<Float> distances;
    Array<Size> idxs;

public:
    /// Constructs boundary conditions with no particles frozen. These can be later added using freeze
    /// function.
    FrozenParticles();

    ~FrozenParticles();

    /// Constructs boundary conditions given domain and search radius (in units of smoothing length) up to
    /// which the particles will be frozen.
    FrozenParticles(std::unique_ptr<Abstract::Domain>&& domain, const Float radius);

    /// Adds body ID particles of which shall be frozen by boundary conditions.
    void freeze(const Size flag);

    /// Remove a body from the list of frozen bodies. If the body is not on the list, nothing happens.
    void thaw(const Size flag);

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override;
};


/// Boundary conditions creating particles with given velocity on one end of the domain and removing them on
/// the other side, while keeping particles inside the domain using either ghost particles or by freezing
/// them. This is fine-tuned for simulations of a meteorite passing through athmosphere.
class WindTunnel : public FrozenParticles {
public:
    WindTunnel(std::unique_ptr<Abstract::Domain>&& domain, const Float radius);

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override;
};

/// Boundary condition moving all particles passed through the domain to the other side of the domain.
class PeriodicDomain : public Abstract::BoundaryConditions {
    /// \todo modify Finder to search periodically in domain. That should be the whole trick
};


/// Helper tool for 1D tests, projects all particles onto a 1D line.
class Projection1D : public Abstract::BoundaryConditions {
private:
    Range domain;
    ArrayView<Vector> r, v;

public:
    /// Constructs using range as 1D domain
    Projection1D(const Range& domain);

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override;
};


NAMESPACE_SPH_END
