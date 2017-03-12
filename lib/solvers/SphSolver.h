#pragma once

#include "solvers/AbstractSolver.h"
#include "solvers/PressureForce.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

class ContinuityFormulation : public Abstract::Formulation {
private:
    /// Force --- computing acceleraon and energy (pressure, stress, AV)
    Array<std::unique_ptr<Abstract::Force>> forces;

public:
    void create(Storage& storage, const GlobalSettings& settings) {
        if (settings.get<DamageEnum>(GlobalSettingsIds::MODEL_DAMAGE) != DamageEnum::NONE) {
        }
    }
};

/// Computes derivatives
class SphSolver : public Abstract::Solver {
private:
    struct ThreadData {
        /// Storage for all temporary data accumulated in main loop of the solver
        Storage accumulated;

        /// Holds all modules that are evaluated in the loop. Modules save data to the \ref accumulated
        /// storage; one module can use multiple buffers (acceleration and energy derivative) and multiple
        /// modules can write into same buffer (different terms in equation of motion).
        /// Modules are evaluated consecutively (within one thread), so this is thread-safe.
        Array<std::shared_ptr<Abstract::Derivative>> derivatives;

        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Cached array of gradients
        Array<Vector> grads;


        /// Adds derivative if not already present. If the derivative is already stored, new one is NOT
        /// created, even if settings are different.
        template <typename TDerivative>
        void addDerivative(const GlobalSettings& settings) {
            for (auto& d : derivatives) {
                if (typeid(d.get()) == typeid(TDerivative*)) {
                    return;
                }
            }
            values.push(std::make_shared<TDerivative>(settings));
        }
    };

    /// Thread-local data
    ThreadLocal<ThreadData> threadData;

    /// Solvers computing quantities. Solvers can register modules in thread-local storage, and use
    /// accumulated values to compute derivatives.
    /// rename SOLVERS!!!
    /// Solvers:
    ///  PressureTerm
    ///  StressTerm
    ///  ArtificialViscosity(ies)
    ///  EnergyDerivative
    ///  DensityDerivative (ContinuityEquation)
    ///
    ///  (Gravity, homogeneous field, harmonic osc...)
    ///
    ///    all terms, modifiers ...
    ///   velocity div, velocity rot, damage (scalar, tensor), yielding (von Mises, Drucker-Prager), ...
    ///
    ///
    /// Abstract::Rheology
    ///  damage->yielding
    ///  damage cannot be baked
    ///
    /// damage cannot be a multiplier because tensor ...
    /// maybe it's overkill to have a generic concept of multipliers, the only one is damage anyway ...
    /// or perhaps density? (rho + rho0?)
    /// in any case, this is only within sph solver, no need to make it super-general ...
    /// Abstract::Modifier, getActualValue() oveload storage? SphStorage, getValue->getActualValue?
    ///  as we don't know if there are modifiers, we have to get all the quantities through modifiers ...
    ///  (we need to somehow propagate modifier list to all modules, that sucks)
    ///     SphStorage!
    /// Modified<Storage>->getValue, getValues, getAll
    ///
    ThreadPool pool;

    std::unique_ptr<Abstract::Formulation> formulation;

    std::unique_ptr<Abstract::Finder> finder;

public:
    SphSolver() {
        storage.forEach([this](ThreadData& data) { formulation.registerThread(data.derivatives) })
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);

        // (re)build neighbour-finding structure
        finder->build(r);

        // initialize all materials (compute pressure, apply yielding and damage, ...)
        for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
            Abstract::Material& material = storage.getMaterial(i);
            material.initialize(storage, stats);
        }

        for (auto& solver : solvers) {
            storage.forEach([this](ThreadData& data) { solvers.initializeThread(data.derivatives) })
        }

        parallelFor(pool, storage, 0, r.size(), [&](const Size n1, const Size n2, ThreadData& data) {
            for (Size i = n1; i < n2; ++i) {
                finder->findNeighbours(i, r[i][H] * kernel, data.neighs);
                data.grads.clear();
                for (auto& n : data.neighs) {
                    data.push(kernel.grad(i, j));
                }
                for (Derivative& d : data.derivatives) {
                    d.sum(i, neighs, grads);
                }
            }
        });

        for (auto& solver : solvers) {
            solver->finalize(storage);
        }
    }
};

NAMESPACE_SPH_END
