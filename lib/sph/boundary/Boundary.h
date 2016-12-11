#pragma once

/// Boundary conditions
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Domain.h"
#include "objects/wrappers/Range.h"
#include "storage/Storage.h"
#include <memory>

/// \todo create some reasonable interface

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class BoundaryConditions : public Polymorphic {
    public:
        virtual void apply(Storage& storage) = 0;
    };
}

/// Functor copying quantities on ghost particles. Vector quantities are copied symmetrically with a respect
/// to the boundary.
struct GhostFunctor {
    Array<int>& idxs;
    Array<int>& ghostIdxs;
    Abstract::Domain& domain;

    template <typename T>
    void operator()(LimitedArray<T>& v, Array<Vector>& UNUSED(r)) {
        for (int i : idxs) {
            v.push(v[i]);
        }
    }

    void operator()(LimitedArray<Vector>& v, Array<Vector>& r) {
        const Float eps = 1.e-3_f * domain.getBoundingRadius();
        for (int i = 0; i < idxs.size(); ++i) {
            const int idx = idxs[i];

            Float length = getLength(v[idx]);
            if (length == 0._f) {
                v.push(v[idx]); // simply copy zero vector
                continue;
            }
            Vector unit = v[idx] / length;
            // approximate inverted vector
            StaticArray<Vector, 1> vgSum{ r[idx] + unit * eps };
            domain.invert(vgSum);
            // position of ghost
            const Vector rg = r[ghostIdxs[i]];
            // get direction of inverted vector as difference
            const Vector diff = vgSum[0] - rg;
            // scale to correct length
            v.push(diff * length / eps);
        }
    }
};

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
    GhostParticles(std::unique_ptr<Abstract::Domain>&& domain, const Settings<GlobalSettingsIds>& settings)
        : domain(std::move(domain)) {
        searchRadius = Factory::getKernel<3>(settings).radius();
    }

    virtual void apply(Storage& storage) override {
        // remove previous ghost particles
        iterate<VisitorEnum::ALL_BUFFERS>(storage, [this](auto&& v) {
            // remove from highest index so that lower indices are unchanged
            for (int i = ghostIdxs.size() - 1; i >= 0; --i) {
                v.remove(ghostIdxs[i]);
            }
        });
        Array<Vector>& r = storage.getValue<Vector>(QuantityKey::R);

        std::cout << "Beginning" << std::endl;
        for (Vector x : r) {
            std::cout << x << std::endl;
        }
        // project particles outside of the domain on the boundary
        /// \todo this will place particles on top of each other, we should probably separate them a little
        domain->project(r);

        std::cout << "After projection" << std::endl;
        for (Vector x : r) {
            std::cout << x << std::endl;
        }

        // find particles close to the boundary
        idxs.clear();
        domain->getDistanceToBoundary(r, distances);
        for (int i = 0; i < r.size(); ++i) {
            if (distances[i] < r[i][H] * searchRadius) {
                // close to boundary, needs a ghost particle
                idxs.push(i);
            }
        }
        ghostIdxs.clear();
        for (int i = r.size(); i < r.size() + idxs.size(); ++i) {
            ghostIdxs.push(i);
        }

        std::cout << "INDICES" << std::endl;
        for (int i : idxs) {
            std::cout << i << std::endl;
        }
        std::cout << "vectors with ghosts" << std::endl;
        for (int i : idxs) {
            std::cout << r[i] << std::endl;
        }

        // create ghost particles
        for (int i : idxs) {
            r.push(r[i]); // copy particle positions
        }

        std::cout << "Added initial ghosts" << std::endl;
        for (Vector x : r) {
            std::cout << x << std::endl;
        }
        // invert particle positions with a respect to the boundary
        domain->invert(r, ghostIdxs);

        std::cout << "After inversion" << std::endl;
        for (Vector x : r) {
            std::cout << x << std::endl;
        }

        // copy values on ghosts
        GhostFunctor functor{ idxs, ghostIdxs, *domain };
        iterateWithPositions(storage, functor);
    }
};

enum class ProjectingOptions {
    ZERO_VELOCITY,      ///< velocities of particles outside of domain are set to zero
    ZERO_PERPENDICULAR, /// < sets perpendicular component of the velocity to zero, parallel remains the same
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
    DomainProjecting(std::unique_ptr<Abstract::Domain>&& domain, const ProjectingOptions options)
        : domain(std::move(domain))
        , options(options) {}

    virtual void apply(Storage& storage) override {
        ArrayView<Vector> r, v;
        tieToArray(r, v) = storage.getAll<Vector>(QuantityKey::R);
        // check which particles are outside of the domain
        domain->getSubset(r, outside, SubsetType::OUTSIDE);
        domain->project(r, outside);
        vproj.clear();
        int idx = 0;
        switch (options) {
        case ProjectingOptions::ZERO_VELOCITY:
            for (int i : outside) {
                v[i] = Vector(0._f);
            }
            break;
        case ProjectingOptions::ZERO_PERPENDICULAR:
            projectVelocity(r, v);
            for (int i : outside) {
                v[i] = 0.5_f * (v[i] + vproj[idx] - r[i]);
            }
            break;
        case ProjectingOptions::REFLECT:
            projectVelocity(r, v);
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
        // invert the sum
        domain->invert(vproj);
    }
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
    Projection1D(const Range& domain)
        : domain(domain) {}

    virtual void apply(Storage& storage) override {
        ArrayView<Vector> dv;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        for (int i = 0; i < r.size(); ++i) {
            // throw away y and z, keep h
            r[i] = Vector(domain.clamp(r[i][0]), 0._f, 0._f, r[i][H]);
            v[i] = Vector(v[i][0], 0._f, 0._f);
        }
        // To get fixed boundary conditions at ends, we need to null all derivatives of first few and last few
        // particles. Number of particles depends on smoothing length.
        iterate<VisitorEnum::FIRST_ORDER>(storage, [](auto&& UNUSED(v), auto&& dv) {
            using Type = typename std::decay_t<decltype(dv)>::Type;
            const int s = dv.size();
            for (int i : { 0, 1, 2, 3, 4, s - 4, s - 3, s - 2, s - 1 }) {
                dv[i] = Type(0._f);
            }
        });
        iterate<VisitorEnum::SECOND_ORDER>(storage, [](auto&& UNUSED(v), auto&& dv, auto&& d2v) {
            using Type = typename std::decay_t<decltype(dv)>::Type;
            const int s = dv.size();
            for (int i : { 0, 1, 2, 3, 4, s - 4, s - 3, s - 2, s - 1 }) {
                dv[i] = Type(0._f);
                d2v[i] = Type(0._f);
            }
        });
    }
};


NAMESPACE_SPH_END
