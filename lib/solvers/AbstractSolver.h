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

namespace Abstract {
    class Solver : public Polymorphic {
    public:
        /// Sets all quantities in the storage required by the solver to their default values. Quantities
        /// already stored in the storage are not changed, only their allowed range is set to the value in
        /// settings. This should only use the given storage and not the state or methods of the solver
        /// itself! (should be used as static function, as static virtual functions are forbidden).
        /// \param storage Storage where all quantities should be placed.
        /// \param domain Domain defining boundaries of the body. This is only necessary for setting up
        ///               particle positions and their masses.
        /// \param settings Settings containing default values and allowed ranges of quantities.
        virtual void setQuantities(Storage& storage,
            const Abstract::Domain& domain,
            const Settings<BodySettingsIds>& settings) const = 0;

        /// Computes derivatives of all time-dependent quantities.
        /// \param storage Storage containing all quantities. All highest order derivatives are guaranteed to
        ///        be set to zero (this is responsibility of TimeStepping).
        virtual void compute(Storage& storage) = 0;

    protected:
        virtual void computeMaterial(Storage& storage);
    };
}

template <int d, typename TForce>
class SolverBase : public Abstract::Solver {
protected:
    std::unique_ptr<Abstract::Finder> finder;

    Array<NeighbourRecord> neighs;

    std::unique_ptr<Abstract::BoundaryConditions> boundary;

    LutKernel<d> kernel;

public:
    SolverBase(const Settings<GlobalSettingsIds>& settings) {
        finder = Factory::getFinder(settings);
        kernel = Factory::getKernel<d>(settings);

        std::unique_ptr<Abstract::Domain> domain = Factory::getDomain(settings);
        boundary = Factory::getBoundaryConditions(settings, std::move(domain));
    }

    /// Sets quantity values and optionally its allowed range to values given by settings.
    template <typename TValue, OrderEnum TOrder>
    void setQuantityImpl(Storage& storage,
        const QuantityKey key,
        const Settings<BodySettingsIds>& settings,
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

    virtual void setQuantities(Storage& storage,
        const Abstract::Domain& domain,
        const Settings<BodySettingsIds>& settings) const override {
        int N; // Final number of particles
        // Particle positions
        if (!storage.has(QuantityKey::R)) {
            PROFILE_SCOPE("ContinuitySolver::setQuantities");
            std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);

            const int n = settings.get<int>(BodySettingsIds::PARTICLE_COUNT);
            // Generate positions of particles
            Array<Vector> r = distribution->generate(n, domain);
            N               = r.size();
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
            const Float rho0   = settings.get<Float>(BodySettingsIds::DENSITY);
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
                [&](const Vector&, const int i) { return get<0>(eos->getPressure(rho[i], u[i])); });
            /// \todo pressure range?
        }

        // sets additional quantities required by force evaluator
        TForce::setQuantities(storage, settings);
    }
};

NAMESPACE_SPH_END
