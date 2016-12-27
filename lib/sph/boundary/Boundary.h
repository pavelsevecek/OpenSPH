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

namespace Abstract {
    class BoundaryConditions : public Polymorphic {
    public:
        virtual void apply(Storage& storage) = 0;
    };
}


/// Add ghost particles symmetrically for each SPH particle close to boundary, copying all quantities to them.
class GhostParticles : public Abstract::BoundaryConditions {
private:
    std::unique_ptr<Abstract::Domain> domain;
    // index where the ghost particles begin (they are always stored successively)
    Array<int> ghostIdxs;
    Array<int> idxs;
    Array<Float> distances;
    Float searchRadius;

public:
    GhostParticles(std::unique_ptr<Abstract::Domain>&& domain, const GlobalSettings& settings);

    virtual void apply(Storage& storage) override;
};

enum class ProjectingOptions {
    ZERO_VELOCITY,      ///< velocities of particles outside of domain are set to zero
    ZERO_PERPENDICULAR, ///< sets perpendicular component of the velocity to zero, parallel remains the same
    REFLECT,            ///< particles 'bounce off' the boundary, the perpendicular component of the velocity
                        ///  changes sign
};

/// Simply project all particles outside of the domain to its boundary.
class DomainProjecting : public Abstract::BoundaryConditions {
private:
    std::unique_ptr<Abstract::Domain> domain;
    Array<int> outside;
    Array<Vector> vproj;
    ProjectingOptions options;

public:
    DomainProjecting(std::unique_ptr<Abstract::Domain>&& domain, const ProjectingOptions options);

    virtual void apply(Storage& storage) override;

private:
    void projectVelocity(ArrayView<const Vector> r, ArrayView<const Vector> v);
};

class PeriodicDomain : public Abstract::BoundaryConditions {
    /// \todo modify Finder to search periodically in domain. That should be the whole trick
};

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
