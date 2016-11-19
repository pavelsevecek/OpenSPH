#include "models/BasicModel.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "sph/distributions/Distribution.h"
#include "storage/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

template <int d>
BasicModel<d>::BasicModel(const std::shared_ptr<Storage>& storage,
                          const Settings<GlobalSettingsIds>& settings)
    : Abstract::Model(storage)
    , monaghanAv(settings) {
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<d>(settings);

    /// \todo we have to somehow connect EoS with storage. Plus when two storages are merged, we have to
    /// remember EoS for each particles. This should be probably generalized, for example we want to remember
    /// original body of the particle, possibly original position (right under surface, core of the body,
    /// antipode, ...)
    eos = Factory::getEos(BODY_SETTINGS);

    std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
    boundary = Factory::getBoundaryConditions(settings, storage, std::move(domain));
}

template <int d>
BasicModel<d>::~BasicModel() {}

template <int d>
void BasicModel<d>::compute(Storage& storage) {
    const int size = storage.getParticleCnt();

    ArrayView<Vector> r, v, dv;
    ArrayView<Float> rho, drho, u, du, p, m, cs;
    {
        PROFILE_SCOPE("BasicModel::compute (getters)")
        // fetch quantities from storage
        refs(r, v, dv) = storage.getAll<QuantityKey::R>();
        refs(rho, drho) = storage.getAll<QuantityKey::RHO>();
        refs(u, du)     = storage.getAll<QuantityKey::U>();
        tie(p, m, cs) = storage.get<QuantityKey::P, QuantityKey::M, QuantityKey::CS>();
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
    }

    {
        PROFILE_SCOPE("BasicModel::compute (init)")
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
        eos->getSoundSpeed(rho, p, cs);
        ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));
        ASSERT(areAllMatching(cs, [](const Float v) { return v > 0._f; }));

        // build neighbour finding object
        /// \todo only rebuild(), try to limit allocations
        /// \todo do it without saving h in r
        /* for (int i=0; i<size; ++i) {
             r[i][H] = h[i];
         }*/
        finder->build(r);
    }
    // we symmetrize kernel by averaging smoothing lenghts
    SymH<d> w(kernel);
    {
        PROFILE_SCOPE("BasicModel::compute (main cycle)")
        for (int i = 0; i < size; ++i) {
            // Find all neighbours within kernel support. Since we are only searching for particles with
            // smaller
            // h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to h_i, and
            // we
            // thus never "miss" a particle.
            SCOPE_STOP;
            finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
            SCOPE_RESUME;
            // iterate over neighbours
            const Float pRhoInvSqr = p[i] / Math::sqr(rho[i]);
            ASSERT(Math::isReal(pRhoInvSqr));
            for (const auto& neigh : neighs) {
                const int j = neigh.index;
                // actual smoothing length
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS && hbar <= r[i][H]);
                if (getSqrLength(r[i] - r[j]) > Math::sqr(kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                // compute gradient of kernel W_ij
                const Vector grad = w.getGrad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);
                // ASSERT(Math::isReal(grad) && getSqrLength(grad) > 0._f);

                // compute forces (accelerations) + artificial viscosity
                const Float av = monaghanAv(v[i] - v[j],
                                            r[i] - r[j],
                                            0.5_f * (cs[i] + cs[j]),
                                            0.5_f * (rho[i] + rho[j]),
                                            hbar);
                const Vector f = (pRhoInvSqr + p[j] / Math::sqr(rho[j]) + av) * grad;
                ASSERT(Math::isReal(f));
                dv[i] -= m[j] * f; // opposite sign due to antisymmetry of gradient
                dv[j] += m[i] * f;

                // compute velocity divergence, save them as 4th computent of velocity vector
                const Float delta = dot(v[j] - v[i], grad);
                ASSERT(Math::isReal(delta));
                divv[i] += m[j] * delta; // same sign as both gradient and (v_i-v_j) are antisymmetric
                divv[j] += m[i] * delta;
            }
        }
    }
    // solve all remaining quantities using computed values
    /// \todo this should be also generalized to some abstract solvers ...
    /// \todo smoothing length should also be solved together with density. This is probably not performance
    /// critical, though.
    {
        PROFILE_SCOPE("BasicModel::compute (solvers)")
        this->solveSmoothingLength(v, dv, r, rho);
        this->solveDensity(drho);
        this->solveEnergy(du, p, rho);

        // Apply boundary conditions
        if (boundary) {
            boundary->apply();
        }
    }
}

template <int d>
void BasicModel<d>::solveSmoothingLength(ArrayView<Vector> v,
                                         ArrayView<Vector> dv,
                                         ArrayView<const Vector> r,
                                         ArrayView<const Float> rho) {
    ASSERT(divv.size() == r.size());
    for (int i = 0; i < r.size(); ++i) {
        v[i][H] = r[i][H] * divv[i] / (Float(d) * rho[i]);
        // clear 'acceleration' of smoothing lengths, we advance h as first-order quantity, even though it is
        // stored within positions.
        dv[i][H] = 0._f;
    }
}

template <int d>
void BasicModel<d>::solveDensity(ArrayView<Float> drho) {
    ASSERT(areAllMatching(drho, [](const Float v) { return v == 0._f; }));
    ASSERT(drho.size() == divv.size());
    for (int i = 0; i < drho.size(); ++i) {
        drho[i] = -divv[i];
    }
}

template <int d>
void BasicModel<d>::solveEnergy(ArrayView<Float> du, ArrayView<const Float> p, ArrayView<const Float> rho) {
    ASSERT(areAllMatching(du, [](const Float v) { return v == 0._f; }));
    for (int i = 0; i < du.size(); ++i) {
        du[i] = -p[i] / Math::sqr(rho[i]) * divv[i];
    }
}

template <int d>
Storage BasicModel<d>::createParticles(const Abstract::Domain& domain,
                                       const Settings<BodySettingsIds>& settings) const {
    PROFILE_SCOPE("BasicModel::createParticles")
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

    const int n = settings.get<int>(BodySettingsIds::PARTICLE_COUNT);
    // Generate positions of particles
    Array<Vector> rs = distribution->generate(n, domain);

    // Final number of particles
    const int N = rs.size();
    ASSERT(N);

    /* // Extract smoothing lengths
     Array<Float> hs;
     for (Vector& r : rs) {
         hs.push(r[H]);
     }*/

    Storage st;

    /*// Same for smoothing lengths
    st.insert<QuantityKey::H>(std::move(hs));*/

    // Create all other quantities (with empty arrays so far)
    st.insert<QuantityKey::R,
              QuantityKey::M,
              QuantityKey::P,
              QuantityKey::RHO,
              QuantityKey::U,
              QuantityKey::CS>();

    // Put generated particles inside the storage.
    st.get<QuantityKey::R>() = std::move(rs);

    // Allocate all arrays
    // Note that this will leave some garbage data in the arrays. We have to make sure the arrays are filled
    // with correct values before we use them. Highest-order derivatives are cleared automatically by
    // timestepping object.
    iterate<TemporalEnum::ALL>(st, [N](auto&& array) { array.resize(N); });
    // Set velocity to zero
    st.dt<QuantityKey::R>().fill(Vector(0._f));

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
    const Float totalM = domain.getVolume() * rho0.get(); // m = rho * V
    ASSERT(totalM > 0._f);
    st.get<QuantityKey::M>().fill(totalM / N);

    // compute pressure and sound speed using equation of state
    std::unique_ptr<Abstract::Eos> bodyEos = Factory::getEos(settings);
    ArrayView<Float> rhos, us, ps, css;
    tie(rhos, us, ps, css) = st.get<QuantityKey::RHO, QuantityKey::U, QuantityKey::P, QuantityKey::CS>();
    bodyEos->getPressure(rhos, us, ps);
    bodyEos->getSoundSpeed(rhos, ps, css);

    return st;
}

/// instantiate classes
template class BasicModel<1>;
template class BasicModel<2>;
template class BasicModel<3>;

NAMESPACE_SPH_END
