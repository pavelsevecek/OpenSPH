#include "models/BasicModel.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "sph/initconds/InitConds.h"
#include "storage/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

BasicModel::BasicModel(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings)
    : Abstract::Model(storage) {
    finder = Factory::getFinder((FinderEnum)settings.get<int>(GlobalSettingsIds::FINDER).get());
    kernel = Factory::getKernel((KernelEnum)settings.get<int>(GlobalSettingsIds::KERNEL).get());

    /// \todo we have to somehow connect EoS with storage. Plus when two storages are merged, we have to
    /// remember EoS for each particles. This should be probably generalized, for example we want to remember
    /// original body of the particle, possibly original position (right under surface, core of the body,
    /// antipode, ...)
    eos = Factory::getEos(BODY_SETTINGS);
}

BasicModel::~BasicModel() {}

NO_INLINE void BasicModel::compute(Storage& storage) {
    PROFILE_SCOPE("BasicModel::compute")

    const int size = storage.getParticleCnt();

    ArrayView<Vector> r, v, dv;
    ArrayView<Float> rho, drho, u, du, p, m;
    // fetch quantities from storage
    refs(r, v, dv) = storage.getAll<QuantityKey::R>();
    refs(rho, drho) = storage.getAll<QuantityKey::RHO>();
    refs(u, du)     = storage.getAll<QuantityKey::U>();
    // refs(h, dh)     = storage.getAll<QuantityKey::H>();
    tie(p, m) = storage.get<QuantityKey::P, QuantityKey::M>();

    // clear velocity divergence (derivatives are already cleared by timestepping)
    divv.resize(r.size());
    divv.fill(0._f);

    // clamp smoothing length
    /// \todo generalize clamping, min / max values
    for (Float& h : componentAdapter(r, H)) {
        h = Math::max(h, EPS);
    }

    // find new pressure
    /// \todo move outside? to something like "poststep" method
    eos->getPressure(rho, u, p);

    // build neighbour finding object
    /// \todo only rebuild(), try to limit allocations
    /// \todo do it without saving h in r
    /* for (int i=0; i<size; ++i) {
         r[i][H] = h[i];
     }*/
    finder->build(r);

    // we symmetrize kernel by averaging smoothing lenghts
    SymH w(kernel);
    for (int i = 0; i < size; ++i) {
        // Find all neighbours within kernel support. Since we are only searching for particles with smaller
        // h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to h_i, and we
        // thus never "miss" a particle.
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);

        // iterate over neighbours
        const Float pRhoInvSqr = p[i] / Math::sqr(rho[i]);
        ASSERT(Math::isReal(pRhoInvSqr));
        for (const auto& neigh : neighs) {
            const int j = neigh.index;
            // actual smoothing length
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            ASSERT(hbar <= r[i][H]);
            if (getSqrLength(r[i] - r[j]) > Math::sqr(kernel.radius() * hbar)) {
                // aren't actual neighbours
                continue;
            }
            // compute gradient of kernel
            const Vector grad = w.getGrad(r[i], r[j]);
            ASSERT(Math::isReal(grad));

            // compute forces (accelerations)
            const Vector f = (pRhoInvSqr + p[j] / Math::sqr(rho[j])) * grad;
            ASSERT(Math::isReal(f));
            dv[i] -= m[j] * f;
            dv[j] += m[i] * f;

            // compute velocity divergence, save them as 4th computent of velocity vector
            const Float delta = dot(v[j] - v[i], grad);
            ASSERT(Math::isReal(delta));
            divv[i] -= m[j] * delta;
            divv[j] += m[i] * delta;
        }
    }
    // solve all remaining quantities using computed values
    /// \todo this should be also generalized to some abstract solvers ...
    /// \todo smoothing length should also be solved together with density. This is probably not performance
    /// critical, though.
    this->solveSmoothingLength(v, dv, r, rho);
    this->solveDensity(drho);
    this->solveEnergy(du, p, rho);
}

void BasicModel::solveSmoothingLength(ArrayView<Vector> v,
                                      ArrayView<Vector> dv,
                                      ArrayView<const Vector> r,
                                      ArrayView<const Float> rho) {
    ASSERT(divv.size() == r.size());
    for (int i = 0; i < r.size(); ++i) {
        v[i][H] = r[i][H] * divv[i] / (3._f * rho[i]);
        // clear 'acceleration' of smoothing lengths, we advance h as first-order quantity, even though it is
        // stored within positions.
        dv[i][H] = 0._f;
    }
}

void BasicModel::solveDensity(ArrayView<Float> drho) {
    ASSERT(drho.size() == divv.size());
    for (int i = 0; i < drho.size(); ++i) {
        drho[i] = -divv[i];
    }
}

void BasicModel::solveEnergy(ArrayView<Float> du, ArrayView<const Float> p, ArrayView<const Float> rho) {
    for (int i = 0; i < du.size(); ++i) {
        du[i] = -p[i] / Math::sqr(rho[i]) * divv[i];
    }
}

Storage BasicModel::createParticles(const int n,
                                    std::unique_ptr<Abstract::Domain> domain,
                                    const Settings<BodySettingsIds>& settings) const {
    PROFILE_SCOPE("BasicModel::createParticles")
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

    // Generate positions of particles
    Array<Vector> rs = distribution->generate(n, domain.get());

    // Final number of particles
    const int N = rs.size();
    ASSERT(N);

    /* // Extract smoothing lengths
     Array<Float> hs;
     for (Vector& r : rs) {
         hs.push(r[H]);
     }*/

    Storage st;

    // Put generated particles inside the storage.
    st.insert<QuantityKey::R>(std::move(rs));

    /*// Same for smoothing lengths
    st.insert<QuantityKey::H>(std::move(hs));*/

    // Create all other quantities (with empty arrays so far)
    st.insert<QuantityKey::M, QuantityKey::P, QuantityKey::RHO, QuantityKey::U>();

    // Allocate all arrays
    // Note: derivatives of positions (velocity, accelerations) are set to 0 by inserting the array. All other
    // quantities are constant or 1st order, so their derivatives will be cleared by timestepping, we don't
    // have to do this here.
    iterate<TemporalEnum::ALL>(st, [N](auto&& array) { array.resize(N); });

    // Set density to default value
    const Optional<Float> rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
    ASSERT(rho0);
    st.get<QuantityKey::RHO>().fill(rho0.get());

    // set internal energy to default value
    const Optional<Float> u0 = settings.get<Float>(BodySettingsIds::ENERGY);
    ASSERT(u0);
    st.get<QuantityKey::U>().fill(u0.get());

    // set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float totalM = domain->getVolume() * rho0.get(); // m = rho * V
    ASSERT(totalM > 0._f);
    st.get<QuantityKey::M>().fill(totalM / N);

    // compute pressure using equation of state
    std::unique_ptr<Abstract::Eos> bodyEos = Factory::getEos(settings);
    ArrayView<Float> rhos, us, ps;
    tie(rhos, us, ps) = st.get<QuantityKey::RHO, QuantityKey::U, QuantityKey::P>();
    bodyEos->getPressure(rhos, us, ps);

    return st;
}


NAMESPACE_SPH_END
