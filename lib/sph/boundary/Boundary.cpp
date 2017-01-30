#include "sph/boundary/Boundary.h"
#include "geometry/Domain.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GhostParticles implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FrozenParticles implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

FrozenParticles::FrozenParticles() = default;

FrozenParticles::~FrozenParticles() = default;

FrozenParticles::FrozenParticles(std::unique_ptr<Abstract::Domain>&& domain, const Float radius)
    : domain(std::move(domain))
    , radius(radius) {}

/// Adds body ID particles of which shall be frozen by boundary conditions.
void FrozenParticles::freeze(const Size flag) {
    frozen.insert(flag);
}

/// Remove a body from the list of frozen bodies. If the body is not on the list, nothing happens.
void FrozenParticles::thaw(const Size flag) {
    frozen.erase(flag);
}

void FrozenParticles::apply(Storage& storage) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);

    idxs.clear();
    if (domain) {
        // project particles outside of the domain onto the boundary
        domain->getSubset(r, idxs, SubsetType::OUTSIDE);
        domain->project(r, idxs.getView());

        // freeze particles close to the boundary
        idxs.clear();
        domain->getDistanceToBoundary(r, distances);
        for (Size i = 0; i < r.size(); ++i) {
            if (distances[i] < radius * r[i][H]) {
                idxs.push(i);
            }
        }
    }

    // find all frozen particles by their body flag
    ArrayView<Size> flags = storage.getValue<Size>(QuantityIds::FLAG);
    for (Size i = 0; i < r.size(); ++i) {
        if (frozen.find(flags[i]) != frozen.end()) {
            idxs.push(i); // this might add particles already frozen by boundary, but it doesn't matter
        }
    }

    // set all highest derivatives of flagged particles to zero
    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(storage, [this](QuantityIds, auto&& d2f) {
        using T = typename std::decay_t<decltype(d2f)>::Type;
        for (Size i : idxs) {
            d2f[i] = T(0._f);
        }
    });
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Projection1D implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


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
