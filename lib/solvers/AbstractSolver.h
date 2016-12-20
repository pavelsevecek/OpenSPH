#pragma once

#include "objects/Object.h"
#include "sph/distributions/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "storage/Storage.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Domain;
    class Finder;
    class BoundaryConditions;
}
struct NeighbourRecord;
class QuantityMap;

namespace Abstract {
    class Solver : public Polymorphic {
    public:
        /// Computes derivatives of all time-dependent quantities.
        /// \param storage Storage containing all quantities. All highest order derivatives are guaranteed to
        ///        be set to zero (this is responsibility of TimeStepping).
        virtual void integrate(Storage& storage) = 0;

        /// Returns the quantity map used by the solver.
        virtual QuantityMap getQuantityMap() const = 0;
    };
}

/// Extended base class for solvers (templated class, cannot be used directly in methods as templated virtual
/// methods do not exist)
template <int D>
class SolverBase : public Abstract::Solver {
protected:
    std::unique_ptr<Abstract::Finder> finder;

    Array<NeighbourRecord> neighs;

    std::unique_ptr<Abstract::BoundaryConditions> boundary;

    static constexpr int dim = D;
    LutKernel<D> kernel;

public:
    SolverBase(const GlobalSettings& settings) {
        finder = Factory::getFinder(settings);
        kernel = Factory::getKernel<D>(settings);

        std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
        boundary = Factory::getBoundaryConditions(settings, std::move(domain));
    }


    /// Sets quantity values and optionally its allowed range to values given by settings.
    template <typename TValue, OrderEnum TOrder>
    void setQuantityImpl(Storage& storage,
        const QuantityKey key,
        const BodySettings& settings,
        const BodySettingsIds valueId,
        const Optional<BodySettingsIds> rangeId) const {
        if (!storage.has(key)) {
            const Float value = settings.get<TValue>(valueId);
            if (rangeId) {
                storage.emplace<TValue, TOrder>(key, value, settings.get<Range>(rangeId.get()));
            } else {
                storage.emplace<TValue, TOrder>(key, value);
            }
            return;
        }
        if (rangeId) {
            // just set the range
            storage.getValue<TValue>(key).setBounds(settings.get<Range>(rangeId.get()));
        }
    }

    /*virtual void setQuantities(Storage& storage,
        const Abstract::Domain& domain,
        const BodySettings& settings) const override {
        int N; // Final number of particles
        // Particle positions
        if (!storage.has(QuantityKey::R)) {
            PROFILE_SCOPE("ContinuitySolver::setQuantities");
            std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

            const int n = settings.get<int>(BodySettingsIds::PARTICLE_COUNT);
            // Generate positions of particles
            Array<Vector> r = distribution->generate(n, domain);
            N = r.size();
            storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::R, std::move(r));
            ASSERT(N > 0);
        } else {
            N = storage.getParticleCnt();
        }

        // Density
        this->template setQuantityImpl<Float, OrderEnum::FIRST_ORDER>(
            storage, QuantityKey::RHO, settings, BodySettingsIds::DENSITY, BodySettingsIds::DENSITY_RANGE);

        // Internal energy
        this->template setQuantityImpl<Float, OrderEnum::FIRST_ORDER>(
            storage, QuantityKey::U, settings, BodySettingsIds::ENERGY, BodySettingsIds::ENERGY_RANGE);

        // Set masses of particles, assuming all particles have the same mass
        /// \todo this has to be generalized when using nonuniform particle destribution
        if (!storage.has(QuantityKey::M)) {
            const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
            const Float totalM = domain.getVolume() * rho0; // m = rho * V
            ASSERT(totalM > 0._f);
            storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::M, totalM / N);
        }

        // Compute pressure using equation of state
        std::unique_ptr<Abstract::Eos> eos = Factory::getEos(settings);
        ArrayView<Float> rho, u, p;
        tieToArray(rho, u) = storage.getValues<Float>(QuantityKey::RHO, QuantityKey::U);
        if (!storage.has(QuantityKey::P)) {
            storage.emplaceWithFunctor<Float, OrderEnum::ZERO_ORDER>(QuantityKey::P,
                [&](const Vector&, const int i) { return eos->getPressure(rho[i], u[i]).get<0>(); });
            /// \todo pressure range?
        }

        // sets additional quantities required by force evaluator
        /// TContext::setQuantities(storage, settings);
    }*/
};

NAMESPACE_SPH_END
