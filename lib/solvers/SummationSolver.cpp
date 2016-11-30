#include "solvers/SummationSolver.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "sph/distributions/Distribution.h"
#include "storage/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

template <int d>
SummationSolver<d>::SummationSolver(const std::shared_ptr<Storage>& storage,
                                    const Settings<GlobalSettingsIds>& settings)
    : Abstract::Solver(storage)
    , monaghanAv(settings) {
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<d>(settings);

    /// \todo we have to somehow connect EoS with storage. Plus when two storages are merged, we have to
    /// remember EoS for each particles. This should be probably generalized, for example we want to remember
    /// original body of the particle, possibly original position (right under surface, core of the body,
    /// antipode, ...)

    /// \todo !!!toto je ono, tady nejde globalni nastaveni
    eos = Factory::getEos(BODY_SETTINGS);

    eta = settings.get<Float>(GlobalSettingsIds::SPH_KERNEL_ETA);

    std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
    boundary = Factory::getBoundaryConditions(settings, std::move(domain));
}

template <int d>
SummationSolver<d>::~SummationSolver() {}

template <int d>
void SummationSolver<d>::compute(Storage& storage) {
    const int size = storage.getParticleCnt();

    ArrayView<Vector> r, v, dv;
    ArrayView<Float> rho, drho, u, du, p, m, cs;
    PROFILE_SCOPE("SummationSolver::compute (getters)");

    // fetch quantities from storage
    refs(r, v, dv) = storage.getAll<QuantityKey::R>();
    refs(rho, drho) = storage.getAll<QuantityKey::RHO>();
    refs(u, du)     = storage.getAll<QuantityKey::U>();
    tie(p, m, cs) = storage.get<QuantityKey::P, QuantityKey::M, QuantityKey::CS>();
    ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));

    PROFILE_NEXT("SummationSolver::compute (init)");
    accumulatedRho.resize(r.size());
    accumulatedRho.fill(0._f);
    accumulatedH.resize(r.size());
    accumulatedH.fill(0._f);

    // clamp smoothing length
    /// \todo generalize clamping, min / max values
    for (Float& h : componentAdapter(r, H)) {
        h = Math::max(h, 1.e-12_f);
    }

    // find new pressure
    /// \todo move outside? to something like "poststep" method
    eos->getPressure(rho, u, p);
    eos->getSoundSpeed(rho, p, cs);
    ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));
    ASSERT(areAllMatching(cs, [](const Float v) { return v > 0._f; }));

    finder->build(r);

    // we symmetrize kernel by averaging kernel values
    SymW<d> w(kernel);

    PROFILE_NEXT("SummationSolver::compute (main cycle)");
    int maxIteration = 0;
    for (int i = 0; i < size; ++i) {
        SCOPE_STOP;
        // find neighbours
        const Float pRhoInvSqr = p[i] / Math::sqr(rho[i]);
        ASSERT(Math::isReal(pRhoInvSqr));
        accumulatedH[i]   = r[i][H];
        Float previousRho = EPS;
        accumulatedRho[i] = rho[i];
        int iterationIdx  = 0;
        while (iterationIdx < 20 && Math::abs(previousRho - accumulatedRho[i]) / previousRho > 1.e-3_f) {
            previousRho = accumulatedRho[i];
            if (iterationIdx == 0 || accumulatedH[i] > r[i][H]) {
                // smoothing length increased, we need to recompute neighbours
                finder->findNeighbours(i, accumulatedH[i] * kernel.radius(), neighs);
            }
            SCOPE_RESUME;
            // find density and smoothing length by self-consistent solution.
            accumulatedRho[i] = 0._f;
            for (const auto& neigh : neighs) {
                const int j = neigh.index;
                accumulatedRho[i] += m[j] * kernel.value(r[i] - r[j], accumulatedH[i]);
            }
            accumulatedH[i] = eta * Math::root<d>(m[i] / accumulatedRho[i]);
            iterationIdx++;
        }
        maxIteration = Math::max(maxIteration, iterationIdx);
        for (const auto& neigh : neighs) {
            const int j = neigh.index;
            // compute gradient of kernel W_ij
            const Vector grad = w.grad(r[i], r[j]);
            ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

            // compute forces (accelerations) + artificial viscosity
            const Float av = monaghanAv(v[i] - v[j],
                                        r[i] - r[j],
                                        0.5_f * (cs[i] + cs[j]),
                                        0.5_f * (rho[i] + rho[j]),
                                        0.5_f * (r[i][H] + r[j][H]));
            const Vector f = (pRhoInvSqr + p[j] / Math::sqr(rho[j]) + av) * grad;
            ASSERT(Math::isReal(f));
            dv[i] -= m[j] * f; // opposite sign due to antisymmetry of gradient

            const Float delta = dot(v[j] - v[i], grad);
            ASSERT(Math::isReal(delta));
            du[i] -= p[i] / Math::sqr(rho[i]) * m[j] * delta - 0.5_f * m[j] * av * delta;
        }
    }
    std::cout << "Max " << maxIteration << " iterations " << std::endl;

    PROFILE_NEXT("SummationSolver::compute (solvers)")

    for (int i = 0; i < size; ++i) {
        rho[i]   = accumulatedRho[i];
        r[i][H]  = accumulatedH[i];
        v[i][H]  = 0._f;
        dv[i][H] = 0._f;
    }

    // Apply boundary conditions
    if (boundary) {
        boundary->apply();
    }
}


template <int d>
Storage SummationSolver<d>::createParticles(const Abstract::Domain& domain,
                                            const Settings<BodySettingsIds>& settings) const {
    PROFILE_SCOPE("SummationSolver::createParticles")
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

    const int n = settings.get<int>(BodySettingsIds::PARTICLE_COUNT);
    // Generate positions of particles
    Array<Vector> rs = distribution->generate(n, domain);

    // Final number of particles
    const int N = rs.size();
    ASSERT(N > 0);

    Storage st;

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
    iterate<VisitorEnum::ALL_BUFFERS>(st, [N](auto&& array) { array.resize(N); });
    // Set velocity to zero
    st.dt<QuantityKey::R>().fill(Vector(0._f));

    // Set density to default value and save the allowed range
    LimitedArray<Float>& rho = st.get<QuantityKey::RHO>();
    const Float rho0         = settings.get<Float>(BodySettingsIds::DENSITY);
    rho.fill(rho0);
    rho.setBounds(settings.get<Range>(BodySettingsIds::DENSITY_RANGE));

    // set internal energy to default value and save the allowed range
    LimitedArray<Float>& u = st.get<QuantityKey::U>();
    u.fill(settings.get<Float>(BodySettingsIds::ENERGY));
    u.setBounds(settings.get<Range>(BodySettingsIds::ENERGY_RANGE));

    // set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float totalM = domain.getVolume() * rho0; // m = rho * V
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
template class SummationSolver<1>;
template class SummationSolver<2>;
template class SummationSolver<3>;

NAMESPACE_SPH_END
