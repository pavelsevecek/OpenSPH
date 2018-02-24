#include "sph/boundary/Boundary.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/Iterators.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "sph/initial/Distribution.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GhostParticles implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

GhostParticles::GhostParticles(AutoPtr<IDomain>&& domain, const Float searchRadius, const Float minimalDist)
    : domain(std::move(domain)) {
    params.searchRadius = searchRadius;
    params.minimalDist = minimalDist;
}

GhostParticles::GhostParticles(AutoPtr<IDomain>&& domain, const RunSettings& settings)
    : domain(std::move(domain)) {
    params.searchRadius = Factory::getKernel<3>(settings).radius();
    params.minimalDist = settings.get<Float>(RunSettingsId::DOMAIN_GHOST_MIN_DIST);
}


/// Functor copying quantities on ghost particles. Vector quantities are copied symmetrically with a respect
/// to the boundary.
struct GhostFunctor {
    Array<Ghost>& ghosts;
    Array<Size>& ghostIdxs;
    IDomain& domain;

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

void GhostParticles::initialize(Storage& storage) {
    ASSERT(ghosts.empty() && ghostIdxs.empty());

    // project particles outside of the domain on the boundary
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    domain->project(r);

    // find particles close to boundary and create necessary ghosts
    domain->addGhosts(r, ghosts, params.searchRadius, params.minimalDist);

    // there is no need to also add particles to dependent storages as we remove ghosts in finalization;
    // however, it might cause problems when there is another BC or equation that DOES need to propagate to
    // dependents. So far no such thing is used, but it would have to be done carefully in case multiple
    // objects add or remove particles from the storage.
    const Size ghostStartIdx = r.size();
    for (Size i = 0; i < ghosts.size(); ++i) {
        ghostIdxs.push(ghostStartIdx + i);
        // push ghosts into the position buffer
        r.push(ghosts[i].position);
    }
    // copy all quantities on ghosts
    GhostFunctor functor{ ghosts, ghostIdxs, *domain };
    iterateWithPositions(storage, functor);

    ASSERT(storage.isValid());

    particleCnt = storage.getParticleCnt();
}

void GhostParticles::finalize(Storage& storage) {
    ASSERT(storage.getParticleCnt() == particleCnt,
        "Solver changed the number of particles. This is currently not consistent with the implementation of "
        "GhostParticles");

    // remove ghosts by indices
    storage.remove(ghostIdxs);
    ghostIdxs.clear();
    ghosts.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FixedParticles implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

FixedParticles::FixedParticles(Params&& params)
    : fixedParticles(std::move(params.material)) {
    ASSERT(isReal(params.thickness));
    Box box = params.domain->getBoundingBox();
    box.extend(box.lower() - Vector(params.thickness));
    box.extend(box.upper() + Vector(params.thickness));

    // We need to fill a layer close to the boundary with dummy particles. IDomain interface does not provide
    // a way to construct enlarged domain, so we need a more general approach - construct a block domain using
    // the bounding box, fill it with particles, and then remove all particles that are inside the original
    // domain or too far from the boundary. This may be inefficient for some obscure domain, but otherwise it
    // works fine.
    BlockDomain boundingDomain(box.center(), box.size());
    /// \todo generalize, we assume that kernel radius = 2 and don't take eta into account
    Array<Vector> dummies =
        params.distribution->generate(box.volume() / pow<3>(0.5_f * params.thickness), boundingDomain);
    // remove all particles inside the actual domain or too far away
    Array<Float> distances;
    params.domain->getDistanceToBoundary(dummies, distances);
    for (Size i = 0; i < dummies.size();) {
        if (distances[i] >= 0._f || distances[i] < -params.thickness) {
            dummies.remove(i);
        } else {
            ++i;
        }
    }
    fixedParticles.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(dummies));

    // create all quantities
    MaterialView mat = fixedParticles.getMaterial(0);
    const Float rho0 = mat->getParam<Float>(BodySettingsId::DENSITY);
    const Float m0 = rho0 * box.volume() / fixedParticles.getParticleCnt();
    fixedParticles.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m0);
    // use negative flag to separate the dummy particles from the real ones
    fixedParticles.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Size(-1));
    params.solver->create(fixedParticles, mat);
    MaterialInitialContext context;
    mat->create(fixedParticles, context);
}

void FixedParticles::initialize(Storage& storage) {
    // add all fixed particles into the storage
    storage.merge(fixedParticles.clone(VisitorEnum::ALL_BUFFERS));
}

void FixedParticles::finalize(Storage& storage) {
    // remove all fixed particles (particles with flag -1) from the storage
    ArrayView<const Size> flag = storage.getValue<Size>(QuantityId::FLAG);
    Array<Size> toRemove;
    for (Size i = 0; i < flag.size(); ++i) {
        if (flag[i] == Size(-1)) {
            toRemove.push(i);
        }
    }

    storage.remove(toRemove, Storage::RemoveFlag::INDICES_SORTED);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FrozenParticles implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

FrozenParticles::FrozenParticles() = default;

FrozenParticles::~FrozenParticles() = default;

FrozenParticles::FrozenParticles(AutoPtr<IDomain>&& domain, const Float radius)
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

void FrozenParticles::finalize(Storage& storage) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    idxs.clear();
    if (domain) {
        // project particles outside of the domain onto the boundary
        domain->getSubset(r, idxs, SubsetType::OUTSIDE);
        domain->project(r, idxs.view());

        // freeze particles close to the boundary
        domain->getDistanceToBoundary(r, distances);
        for (Size i = 0; i < r.size(); ++i) {
            ASSERT(distances[i] >= -EPS);
            if (distances[i] < radius * r[i][H]) {
                idxs.push(i);
            }
        }
    }

    if (!frozen.empty()) {
        // find all frozen particles by their body flag
        ArrayView<Size> flags = storage.getValue<Size>(QuantityId::FLAG);
        for (Size i = 0; i < r.size(); ++i) {
            if (frozen.find(flags[i]) != frozen.end()) {
                idxs.push(i); // this might add particles already frozen by boundary, but it doesn't matter
            }
        }
    }

    // set all highest derivatives of flagged particles to zero
    iterate<VisitorEnum::HIGHEST_DERIVATIVES>(storage, [this](QuantityId, auto& d2f) {
        using T = typename std::decay_t<decltype(d2f)>::Type;
        for (Size i : idxs) {
            d2f[i] = T(0._f);
        }
    });
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// WindTunnel implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

WindTunnel::WindTunnel(AutoPtr<IDomain>&& domain, const Float radius)
    : FrozenParticles(std::move(domain), radius) {}

void WindTunnel::finalize(Storage& storage) {
    // clear derivatives of particles close to boundary
    FrozenParticles::finalize(storage);

    // remove particles outside of the domain
    Array<Size> toRemove;
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        if (!this->domain->contains(r[i])) {
            toRemove.push(i);
        }
    }
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [&toRemove](auto& v) {
        for (Size idx : reverse(toRemove)) { // iterate in reverse to remove higher indices first
            v.remove(idx);
        }
    });

    // find z positions of upper two layers of particles
    /* Float z1 = -INFTY, z2 = -INFTY;
     for (Size i = 0; i < r.size(); ++i) {
         if (r[i][Z] > z1) {
             z2 = z1;
             z1 = r[i][Z];
         }
     }
     ASSERT(z2 > -INFTY && z1 > z2 + 0.1_f * r[0][H]);
     const Float dz = z1 - z2;
     if (z1 + 2._f * dz < this->domain->getBoundingBox().upper()[Z]) {
         // find indices of upper two layers
         Array<Size> idxs;
         const Size size = r.size();
         for (Size i = 0; i < size; ++i) {
             if (r[i][Z] >= z2) {
                 idxs.push(i);
             }
         }
         ASSERT(!idxs.empty());
         // copy all quantities
         iterate<VisitorEnum::ALL_BUFFERS>(storage, [&idxs](auto& v) {
             for (Size i : idxs) {
                 auto cloned = v[i];
                 v.push(cloned);
             }
         });
         // move created particles by 2dz
         const Vector offset(0._f, 0._f, 2._f * dz);
         for (Size i = size; i < r.size(); ++i) {
             r[i] += offset;
         }
     }*/
    ASSERT(storage.isValid());
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Projection1D implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


Projection1D::Projection1D(const Interval& domain)
    : domain(domain) {}

void Projection1D::finalize(Storage& storage) {
    ArrayView<Vector> dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        // throw away y and z, keep h
        r[i] = Vector(domain.clamp(r[i][0]), 0._f, 0._f, r[i][H]);
        v[i] = Vector(v[i][0], 0._f, 0._f);
    }
    // To get fixed boundary conditions at ends, we need to null all derivatives of first few and last few
    // particles. Number of particles depends on smoothing length.
    iterate<VisitorEnum::FIRST_ORDER>(storage, [](const QuantityId UNUSED(id), auto&& UNUSED(v), auto&& dv) {
        using Type = typename std::decay_t<decltype(dv)>::Type;
        const Size s = dv.size();
        for (Size i : { 0u, 1u, 2u, 3u, 4u, s - 4, s - 3, s - 2, s - 1 }) {
            dv[i] = Type(0._f);
        }
    });
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [](const QuantityId UNUSED(id), auto&& UNUSED(v), auto&& dv, auto&& d2v) {
            using Type = typename std::decay_t<decltype(dv)>::Type;
            const Size s = dv.size();
            for (Size i : { 0u, 1u, 2u, 3u, 4u, s - 4, s - 3, s - 2, s - 1 }) {
                dv[i] = Type(0._f);
                d2v[i] = Type(0._f);
            }
        });
}

NAMESPACE_SPH_END
