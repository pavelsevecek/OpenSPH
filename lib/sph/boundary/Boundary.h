#pragma once

/// Boundary conditions
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Domain.h"
#include "objects/wrappers/Range.h"
#include "quantities/Storage.h"
#include <memory>

/// \todo create some reasonable interface

NAMESPACE_SPH_BEGIN

template <typename TEnum>
class Settings;

enum class GlobalSettingsIds;
using GlobalSettings = Settings<GlobalSettingsIds>;


namespace Abstract {
    class BoundaryConditions : public Polymorphic {
    public:
        virtual void apply(Storage& storage) = 0;
    };
}


/// Adds ghost particles symmetrically for each SPH particle close to boundary. All physical quantities are
/// copied on them. This acts as a natural boundary for SPH particles without creating unphysical gradients
/// due to discontinuity.
class GhostParticles : public Abstract::BoundaryConditions {
private:
    std::unique_ptr<Abstract::Domain> domain;
    // index where the ghost particles begin (they are always stored successively)
    Array<Abstract::Domain::Ghost> ghosts;
    Array<Size> ghostIdxs; // indices of ghost particles in the storage
    Array<Float> distances;
    Float searchRadius;
    Float minimalDist;

public:
    GhostParticles(std::unique_ptr<Abstract::Domain>&& domain, const GlobalSettings& settings);

    /// \todo we hold indices into the storage we get from parameter. There is no guarantee the storage is the
    /// same each time and that it wasn't changed. Think of a better way of doing this, possibly by creating
    /// helper quantity (index of source particle for ghosts and something like -1 for regular particles.
    virtual void apply(Storage& storage) override;

    /// Removes all ghost particles from the storage. This does not have to be called before apply, ghosts are
    /// removed and added automatically.
    /// \todo Currently needs to be called before data output as ghosts would be saved together with "regular"
    ///       particles. This shouldn't happen, ghosts should be removed automatically.
    void removeGhosts(Storage& storage);
};


enum class ProjectingOptions {
    ZERO_VELOCITY,      ///< velocities of particles outside of domain are set to zero
    ZERO_PERPENDICULAR, ///< sets perpendicular component of the velocity to zero, parallel remains the same
    REFLECT,            ///< particles 'bounce off' the boundary, the perpendicular component of the velocity
                        ///  changes sign
};

/// Boundary condition that simply projects all particles outside of the domain to its boundary. Should not be
/// used as main boundary condition in SPH run due to very high gradients created at the boundary. It can be
/// useful as auxiliary tool for setting up initial conditions, for example.
class DomainProjecting : public Abstract::BoundaryConditions {
private:
    std::unique_ptr<Abstract::Domain> domain;
    Array<Size> outside;
    Array<Vector> vproj;
    ProjectingOptions options;

public:
    DomainProjecting(std::unique_ptr<Abstract::Domain>&& domain, const ProjectingOptions options);

    virtual void apply(Storage& storage) override;

private:
    void projectVelocity(ArrayView<const Vector> r, ArrayView<const Vector> v);
};


/// Zeros out all highest-order derivatives of all particles with given index. This is completely independent
/// of domain.
class FixedBody : public Abstract::BoundaryConditions {
public:
    FixedBody(const int matId);

    virtual void apply(Storage& storage) override;
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

    virtual void apply(Storage& storage) override;
};


NAMESPACE_SPH_END
