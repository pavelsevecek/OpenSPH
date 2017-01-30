#include "sph/boundary/Boundary.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"
#include "geometry/Domain.h"

NAMESPACE_SPH_BEGIN

GhostParticles::GhostParticles(std::unique_ptr<Abstract::Domain>&& domain, const GlobalSettings& settings)
    : domain(std::move(domain)) {
    searchRadius = Factory::getKernel<3>(settings).radius();
    minimalDist = settings.get<Float>(GlobalSettingsIds::DOMAIN_GHOST_MIN_DIST);
}


/// Functor copying quantities on ghost particles. Vector quantities are copied symmetrically with a respect
/// to the boundary.
struct GhostFunctor {
    Array<Ghost>& ghosts;
    Array<Size>& ghostIdxs;
    Abstract::Domain& domain;

    /// Generic operator, simply copies value onto the ghost
    template <typename T>
    void operator()(Array<T>& v, Array<Vector>& UNUSED(r)) {
        for (auto& g : ghosts) {
            auto ghost = v[g.index];
            v.push(ghost);
        }
    }

    /// Specialization for vectors, copies parallel component of the vector along the boundary and inverts
    /// perpendicular component.
    void operator()(Array<Vector>& v, Array<Vector>& r) {
        for (auto& g : ghosts) {
            Float length = getLength(v[g.index]);
            if (length == 0._f) {
                v.push(Vector(0._f)); // simply copy zero vector
                continue;
            }
            const Vector deltaR = r[g.index] - g.position; // offset between particle and its ghost
            ASSERT(getLength(deltaR) > 0.f);
            /*if (getLength(deltaR) == 0._f) {
                // ghost lie on top of the particle, approximate inverted vector by finite differences
                // (imprecise)
                /// \todo we should avoid this case altogether
                const Float eps = 1.e-3_f * domain.getBoundingRadius();
                const Vector unit = v[idx] / length;
                StaticArray<Vector, 1> vgSum{ r[idx] + unit * eps };
                domain.invert(vgSum);
                const Vector rg = r[ghostIdxs[i]];
                // get direction of inverted vector as difference inverted(r) and inverted(r + v)
                const Vector diff = getNormalized(vgSum[0] - rg);
                // scale to correct length
                v.push(diff * length);
            } else {*/
            const Vector normal = getNormalized(deltaR);
            const Float perp = dot(normal, v[g.index]);
            // mirror vector by copying parallel component and inverting perpendicular component
            v.push(v[g.index] - 2._f * normal * perp);
        }
    }
};

void GhostParticles::apply(Storage& storage) {
    // remove previous ghost particles
    removeGhosts(storage);

    // project particles outside of the domain on the boundary
    Array<Vector>& r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    domain->project(r);

    // find particles close to boundary and create necessary ghosts
    domain->addGhosts(r, ghosts, searchRadius, minimalDist);

    const Size ghostStartIdx = r.size();
    for (Size i = 0; i < ghosts.size(); ++i) {
        ghostIdxs.push(ghostStartIdx + i);
        r.push(ghosts[i].position);
    }

    // copy all quantities on ghosts
    GhostFunctor functor{ ghosts, ghostIdxs, *domain };
    iterateWithPositions(storage, functor);
}

void GhostParticles::removeGhosts(Storage& storage) {
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [this](auto&& v) {
        // remove from highest index so that lower indices are unchanged
        for (int i = ghostIdxs.size() - 1; i >= 0; --i) {
            v.remove(ghostIdxs[i]);
        }
    });
    ghostIdxs.clear();
    ghosts.clear();
}


DomainProjecting::DomainProjecting(std::unique_ptr<Abstract::Domain>&& domain,
    const ProjectingOptions options)
    : domain(std::move(domain))
    , options(options) {}

void DomainProjecting::apply(Storage& storage) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    // check which particles are outside of the domain
    domain->getSubset(r, outside, SubsetType::OUTSIDE);
    domain->project(r, outside.getView());
    vproj.clear();
    // Size idx = 0;
    switch (options) {
    case ProjectingOptions::ZERO_VELOCITY:
        for (Size i : outside) {
            v[i] = Vector(0._f);
        }
        break;
    default:
        NOT_IMPLEMENTED;
    }
    /* case ProjectingOptions::ZERO_PERPENDICULAR:
         projectVelocity(r, v);
         for (Size i : outside) {
             v[i] = 0.5_f * (v[i] + vproj[idx] - r[i]);
         }
         break;
     case ProjectingOptions::REFLECT:
         projectVelocity(r, v);
         for (Size i : outside) {
             // subtract the original position and we have projected velocities! Yay!)
             v[i] = vproj[idx] - r[i];
         }
         break;
     }*/
}

/*void DomainProjecting::projectVelocity(ArrayView<const Vector> r, ArrayView<const Vector> v) {
    /// \todo implement using normal to the boundary
    for (Size i : outside) {
        // sum up position and velocity
        vproj.push(r[i] + v[i]);
    }
    // invert the sum
    domain->invert(vproj);
}*/

Projection1D::Projection1D(const Range& domain)
    : domain(domain) {}

void Projection1D::apply(Storage& storage) {
    ArrayView<Vector> dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        // throw away y and z, keep h
        r[i] = Vector(domain.clamp(r[i][0]), 0._f, 0._f, r[i][H]);
        v[i] = Vector(v[i][0], 0._f, 0._f);
    }
    // To get fixed boundary conditions at ends, we need to null all derivatives of first few and last few
    // particles. Number of particles depends on smoothing length.
    iterate<VisitorEnum::FIRST_ORDER>(storage, [](const QuantityIds UNUSED(id), auto&& UNUSED(v), auto&& dv) {
        using Type = typename std::decay_t<decltype(dv)>::Type;
        const Size s = dv.size();
        for (Size i : { 0u, 1u, 2u, 3u, 4u, s - 4, s - 3, s - 2, s - 1 }) {
            dv[i] = Type(0._f);
        }
    });
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [](const QuantityIds UNUSED(id), auto&& UNUSED(v), auto&& dv, auto&& d2v) {
            using Type = typename std::decay_t<decltype(dv)>::Type;
            const Size s = dv.size();
            for (Size i : { 0u, 1u, 2u, 3u, 4u, s - 4, s - 3, s - 2, s - 1 }) {
                dv[i] = Type(0._f);
                d2v[i] = Type(0._f);
            }
        });
}

NAMESPACE_SPH_END
