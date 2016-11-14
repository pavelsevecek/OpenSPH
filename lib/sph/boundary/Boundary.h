#pragma once

/// Boundary conditions
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Domain.h"
#include "storage/Storage.h"
#include <memory>

/// \todo create some reasonable interface

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class BoundaryConditions : public Polymorphic {
    protected:
        std::shared_ptr<Storage> storage;
        std::unique_ptr<Abstract::Domain> domain;

    public:
        BoundaryConditions(const std::shared_ptr<Storage>& storage,
                           const std::unique_ptr<Abstract::Domain> domain)
            : storage(storage)
            , domain(domain) {}

        /// Applies boundary conditions onto the particles
        virtual void apply() = 0;
    };
}

/// Add ghost particles symmetrically for each SPH particle close to boundary, copying all quantities to them.
class GhostParticles : public Abstract::BoundaryConditions {
private:
    // indices into storage marking ghost particles. MUST always be after regular particles
    Array<int> indices;

public:
    /*beforeStep
        add particles

    afterStep
        nothing*/
};

enum class ProjectingOptions {
    ZERO_VELOCITY,      ///< velocities of particles outside of domain are set to zero
    ZERO_PERPENDICULAR, /// < sets perpendicular component of the velocity to zero, parallel remains the same
    REFLECT,            ///< particles 'bounce off' the boundary, the perpendicular component of the velocity
                        ///  changes sign
};

/// Simply project all particles outside of the domain to its boundary.
class ParticleProjecting : public Abstract::BoundaryConditions {
private:
    ProjectingOptions options;
    Array<bool> outside;
    Array<Vector> vproj;

public:
    ParticleProjecting(const std::shared_ptr<Storage>& storage,
                       const std::unique_ptr<Abstract::Domain> domain,
                       const ProjectingOptions options)
        : Abstract::BoundaryConditions(storage, domain)
        , options(options) {}

    virtual void apply() override {
        Array<Vector>& r = this->storage->get<QuantityKey::R>();
        Array<Vector>& v = this->storage->dt<QuantityKey::R>();
        // check which particles are outside of the domain
        domain->getSubset(r, outside, SubsetType::OUTSIDE);
        domain->project(r, outside);
        vproj.clear();
        switch (options) {
        case ProjectingOptions::ZERO_VELOCITY:
            for (int i : outside) {
                v[i] = Vector(0._f);
            }
            break;
        case ProjectingOptions::ZERO_PERPENDICULAR:
            projectVelocity(r, v);
            int idx = 0;
            for (int i : outside) {
                v[i] = 0.5_f * (v[i] + vproj[idx] - r[i]);
            }
            break;
        case ProjectingOptions::REFLECT:
            projectVelocity(r, v);
            int idx = 0;
            for (int i : outside) {
                // subtract the original position and we have projected velocities! Yay!)
                v[i] = vproj[idx] - r[i];
            }
            break;
        }
    }
private:
    void projectVelocity(ArrayView<const Vector> r, ArrayView<const Vector> v) {
        /// \todo implement using normal to the boundary
        for (int i : outside) {
            // sum up position and velocity
            vproj.push(r[i] + v[i]);
        }
        // project the sum
        domain->project(vproj);

    }
};

class PeriodicDomain : public Abstract::BoundaryConditions {
    /// \todo modify Finder to search periodically in domain. That should be the whole trick
};

NAMESPACE_SPH_END
