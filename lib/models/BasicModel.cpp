#include "models/BasicModel.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "sph/initconds/InitConds.h"
#include "storage/Iterate.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

BasicModel::BasicModel(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings)
    : Abstract::Model(storage) {
    finder = Factory::getFinder((FinderEnum)settings.get<int>(GlobalSettingsIds::FINDER).get());
    kernel = Factory::getKernel((KernelEnum)settings.get<int>(GlobalSettingsIds::KERNEL).get());

    /// \todo we have to somehow connect EoS with storage. Plus when two storages are merged, we have to remember EoS for each particles. This should be probably generalized, for example we want to remember original body of the particle, possibly original position (right under surface, core of the body, antipode, ...)
    eos = Factory::getEos(BODY_SETTINGS);
}

BasicModel::~BasicModel() {}

void BasicModel::compute(Storage& storage) {
    const int size = storage.getParticleCnt();

    ArrayView<Vector> r, v, dv;
    ArrayView<Float> rho, drho, u, du, p, m;
    // fetch quantities from storage
    refs(r, v, dv) = storage.getAll<QuantityKey::R>();
    refs(rho, drho) = storage.getAll<QuantityKey::RHO>();
    refs(u, du)     = storage.getAll<QuantityKey::U>();
    tie(p, m)       = storage.get<QuantityKey::P, QuantityKey::M>();

    // clear velocity divergence (derivatives are already cleared by timestepping)
    divv.resize(r.size());
    divv.fill(0._f);

    // find new pressure
    /// \todo move outside? to something like "poststep" method
    eos->getPressure(rho, u, p);

    // build neighbour finding object
    finder->build(r);

    // we symmetrize kernel by averaging smoothing lenghts
    SymH w(kernel);
    for (int i = 0; i < size; ++i) {
        // Find all neighbours within kernel support. Since we are only searching for particles with smaller
        // h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to h_i, and we
        // thus never "miss" a particle.
        const int n =
            finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);

        // iterate over neighbours
        const Float pRhoInvSqr = p[i] / Math::sqr(rho[i]);
        for (int j = 0; j < n; ++j) {
            ASSERT(0.5_f * (r[i][H] + r[j][H]) <= r[i][H]);
            // compute gradient of kernel
            const Vector grad = w.getGrad(r[i], r[j]);

            // compute forces (accelerations)
            const Vector f = (pRhoInvSqr + p[j] / Math::sqr(rho[j])) * grad;
            dv[i] -= m[j] * f;
            dv[j] += m[i] * f;

            // compute velocity divergence, save them as 4th computent of velocity vector
            const Float u = dot(v[j] - v[i], grad);
            divv[i] -= m[j] * u;
            divv[j] += m[i] * u;
        }
    }
    // solve all remaining quantities using computed values
    /// \todo this should be also generalized to some abstract solvers ...
    /// \todo smoothing length should also be solved together with density. This is probably not performance critical, though.
    this->solveSmoothingLength(v, r);
    this->solveDensity(drho, rho);
    this->solveEnergy(du, p, rho);
}

void BasicModel::solveSmoothingLength(ArrayView<Vector> v, ArrayView<const Vector> r) {
    ASSERT(divv.size() == r.size());
    constexpr Float dimInv = 1._f / 3._f;
    for (int i = 0; i < r.size(); ++i) {
        v[i][H] = r[i][H] * divv[i] * dimInv;
    }
}

void BasicModel::solveDensity(ArrayView<Float> drho, ArrayView<const Float> rho) {
    ASSERT(drho.size() == rho.size());
    ASSERT(drho.size() == divv.size());
    for (int i = 0; i < drho.size(); ++i) {
        drho[i] = -rho[i] * divv[i];
    }
}

void BasicModel::solveEnergy(ArrayView<Float> du, ArrayView<const Float> p, ArrayView<const Float> rho) {
    for (int i = 0; i < du.size(); ++i) {
        du[i] = -p[i] / rho[i] *divv[i];
    }
}

Storage BasicModel::createParticles(const int n,
                                    std::unique_ptr<Abstract::Domain> domain,
                                    const Settings<BodySettingsIds>& settings) const {
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

    // Generate positions of particles
    Array<Vector> rs = distribution->generate(n, domain.get());

    // Final number of particles
    const int N = rs.size();

    Storage st;

    // Put generated particles inside the storage.
    st.insert<QuantityKey::R>(std::move(rs));

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
