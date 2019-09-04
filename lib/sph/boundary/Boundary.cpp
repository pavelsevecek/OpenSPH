#include "sph/boundary/Boundary.h"
#include "objects/finders/PeriodicFinder.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/Iterator.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "sph/initial/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// GhostParticles implementation
//-----------------------------------------------------------------------------------------------------------

GhostParticles::GhostParticles(SharedPtr<IDomain> domain, const Float searchRadius, const Float minimalDist)
    : domain(std::move(domain)) {
    ASSERT(this->domain);
    params.searchRadius = searchRadius;
    params.minimalDist = minimalDist;
}

GhostParticles::GhostParticles(SharedPtr<IDomain> domain, const RunSettings& settings)
    : domain(std::move(domain)) {
    ASSERT(this->domain);
    params.searchRadius = Factory::getKernel<3>(settings).radius();
    params.minimalDist = settings.get<Float>(RunSettingsId::DOMAIN_GHOST_MIN_DIST);
}

void GhostParticles::initialize(Storage& storage) {
    // note that there is no need to also add particles to dependent storages as we remove ghosts in
    // finalization; however, it might cause problems when there is another BC or equation that DOES need to
    // propagate to dependents. So far no such thing is used, but it would have to be done carefully in case
    // multiple objects add or remove particles from the storage.

    storage.setUserData(nullptr); // clear previous data

    ASSERT(ghosts.empty() && ghostIdxs.empty());

    // project particles outside of the domain on the boundary
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    domain->project(r);

    // find particles close to boundary and create necessary ghosts
    domain->addGhosts(r, ghosts, params.searchRadius, params.minimalDist);

    // extract the indices for duplication
    Array<Size> idxs;
    for (Ghost& g : ghosts) {
        idxs.push(g.index);
    }
    // duplicate all particles that create ghosts to make sure we have correct materials in the storage
    ghostIdxs = storage.duplicate(idxs);

    // set correct positions of ghosts
    ASSERT(ghostIdxs.size() == ghosts.size());
    for (Size i = 0; i < ghosts.size(); ++i) {
        const Size ghostIdx = ghostIdxs[i];
        r[ghostIdx] = ghosts[i].position;
    }

    // reflect velocities (only if we have velocities, GP are also used in Diehl's distribution, for example)
    if (storage.has<Vector>(QuantityId::POSITION, OrderEnum::SECOND)) {
        Array<Vector>& v = storage.getDt<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < ghosts.size(); ++i) {
            const Size ghostIdx = ghostIdxs[i];
            // offset between particle and its ghost
            const Vector deltaR = r[ghosts[i].index] - ghosts[i].position;
            ASSERT(getLength(deltaR) > 0.f);
            const Vector normal = getNormalized(deltaR);
            const Float perp = dot(normal, v[ghosts[i].index]);
            // mirror vector by copying parallel component and inverting perpendicular component
            const Vector v0 = v[ghostIdx] - 2._f * normal * perp;
            if (ghostVelocity) {
                v[ghostIdx] = ghostVelocity(ghosts[i].position).valueOr(v0);
            } else {
                v[ghostIdx] = v0;
            }
            ASSERT(getLength(v[ghostIdx]) < 1.e50_f);
        }
    }

    // set flag to some special value to separate the bodies
    if (storage.has(QuantityId::FLAG)) {
        ArrayView<Size> flag = storage.getValue<Size>(QuantityId::FLAG);
        for (Size i = 0; i < ghosts.size(); ++i) {
            const Size ghostIdx = ghostIdxs[i];
            flag[ghostIdx] = Size(-1);
        }
    }

    ASSERT(storage.isValid());

    particleCnt = storage.getParticleCnt();
}

void GhostParticles::setVelocityOverride(Function<Optional<Vector>(const Vector& r)> newGhostVelocity) {
    ghostVelocity = newGhostVelocity;
}

void GhostParticles::finalize(Storage& storage) {
    ASSERT(storage.getParticleCnt() == particleCnt,
        "Solver changed the number of particles. This is currently not consistent with the implementation of "
        "GhostParticles");

    // remove ghosts by indices
    storage.remove(ghostIdxs);
    ghostIdxs.clear();

    storage.setUserData(makeShared<GhostParticlesData>(std::move(ghosts)));
}

//-----------------------------------------------------------------------------------------------------------
// FixedParticles implementation
//-----------------------------------------------------------------------------------------------------------

FixedParticles::FixedParticles(const RunSettings& settings, Params&& params)
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
    Array<Vector> dummies = params.distribution->generate(
        SEQUENTIAL, box.volume() / pow<3>(0.5_f * params.thickness), boundingDomain);
    // remove all particles inside the actual domain or too far away
    /*Array<Float> distances;
    params.domain->getDistanceToBoundary(dummies, distances);*/
    for (Size i = 0; i < dummies.size();) {
        if (params.domain->contains(dummies[i])) {
            // if (distances[i] >= 0._f || distances[i] < -params.thickness) {
            dummies.remove(i);
        } else {
            ++i;
        }
    }
    // although we never use the derivatives, the second order is needed to properly merge the storages
    fixedParticles.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(dummies));

    // create all quantities
    MaterialView mat = fixedParticles.getMaterial(0);
    const Float rho0 = mat->getParam<Float>(BodySettingsId::DENSITY);
    const Float m0 = rho0 * box.volume() / fixedParticles.getParticleCnt();
    fixedParticles.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m0);
    // use negative flag to separate the dummy particles from the real ones
    fixedParticles.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Size(-1));

    AutoPtr<ISolver> solver = Factory::getSolver(SEQUENTIAL, settings);
    solver->create(fixedParticles, mat);
    MaterialInitialContext context(settings);
    mat->create(fixedParticles, context);
}

void FixedParticles::initialize(Storage& storage) {
    // add all fixed particles into the storage
    storage.merge(fixedParticles.clone(VisitorEnum::ALL_BUFFERS));
    ASSERT(storage.isValid());
    ASSERT(storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS).size() ==
           storage.getValue<Vector>(QuantityId::POSITION).size());
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

    storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);
    ASSERT(storage.isValid());
}

//-----------------------------------------------------------------------------------------------------------
// FrozenParticles implementation
//-----------------------------------------------------------------------------------------------------------

FrozenParticles::FrozenParticles() = default;

FrozenParticles::~FrozenParticles() = default;

FrozenParticles::FrozenParticles(SharedPtr<IDomain> domain, const Float radius)
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

//-----------------------------------------------------------------------------------------------------------
// WindTunnel implementation
//-----------------------------------------------------------------------------------------------------------

WindTunnel::WindTunnel(SharedPtr<IDomain> domain, const Float radius)
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
    iterate<VisitorEnum::ALL_BUFFERS>(storage, [&toRemove](auto& v) { v.remove(toRemove); });

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

//-----------------------------------------------------------------------------------------------------------
// PeriodicBoundary implementation
//-----------------------------------------------------------------------------------------------------------


PeriodicBoundary::PeriodicBoundary(const Box& domain, AutoPtr<IBoundaryCondition>&& additional)
    : domain(domain)
    , additional(std::move(additional)) {}

void PeriodicBoundary::initialize(Storage& storage) {
    ArrayView<Vector> positions = storage.getValue<Vector>(QuantityId::POSITION);
    for (Vector& pos : positions) {
        const Vector lowerFlags(int(pos[X] < domain.lower()[X]),
            int(pos[Y] < domain.lower()[Y]),
            int(pos[Z] < domain.lower()[Z]));
        const Vector upperFlags(int(pos[X] > domain.upper()[X]),
            int(pos[Y] > domain.upper()[Y]),
            int(pos[Z] > domain.upper()[Z]));

        pos += domain.size() * (lowerFlags - upperFlags);
    }

    if (additional) {
        additional->initialize(storage);
    }
}

void PeriodicBoundary::finalize(Storage& storage) {
    if (additional) {
        additional->finalize(storage);
    }
}

AutoPtr<ISymmetricFinder> PeriodicBoundary::getPeriodicFinder(AutoPtr<ISymmetricFinder>&& finder) {
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());
    return makeAuto<PeriodicFinder>(std::move(finder), domain, scheduler);
}

//-----------------------------------------------------------------------------------------------------------
// Projection1D implementation
//-----------------------------------------------------------------------------------------------------------


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
