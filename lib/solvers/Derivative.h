#pragma once

/// Solver for terms due to pressure gradient in equation of motion and energy equation

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "objects/finders/AbstractFinder.h"
#include "quantities/Storage.h"
#include "solvers/Accumulated.h"
#include "system/Settings.h"
#include <map>

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// Derivative accumulated by summing up neighbouring particles. If solver is parallelized, each thread
    /// has its own derivatives that are summed after the solver loop.
    class Derivative : public Polymorphic {
    public:
        /// Initialize derivative before iterating over neighbours.
        /// \param input Storage containing all the input quantities from which derivatives are computed.
        ///              This storage is shared for all threads.
        /// \param results Thread-local storage where the computed derivatives are saved.
        virtual void initialize(const Storage& input, Accumulated& results) = 0;

        /// Compute derivatives from interaction of particle pairs. Each particle pair is visited only once,
        /// the derivatives shall be computed for both particles.
        /// \param idx Index of first interacting particle
        /// \param neighs Array of neighbours of idx-th particle
        /// \param grads Computed gradients of the SPH kernel
        virtual void compute(const Size idx,
            ArrayView<const NeighbourRecord> neighs,
            ArrayView<const Vector> grads) = 0;
    };
}

class VelocityDivergence : public Abstract::Derivative {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> v;
    ArrayView<Float> divv;

public:
    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityIds::DENSITY, QuantityIds::MASSES);
        v = input.getDt<Vector>(QuantityIds::POSITIONS);
        divv = results.getValue<Float>(QuantityIds::VELOCITY_DIVERGENCE);
    }

    virtual void compute(const Size i,
        ArrayView<const NeighbourRecord> neighs,
        ArrayView<const Vector> grads) override {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            const Float proj = dot(v[i] - v[j], grads[i]);
            divv[i] += m[j] / rho[j] * proj;
            divv[j] += m[i] / rho[i] * proj;
        }
    }
};

class DerivativeHolder {
private:
    Accumulated accumulated;
    /// Holds all modules that are evaluated in the loop. Modules save data to the \ref accumulated
    /// storage; one module can use multiple buffers (acceleration and energy derivative) and multiple
    /// modules can write into same buffer (different terms in equation of motion).
    /// Modules are evaluated consecutively (within one thread), so this is thread-safe.
    Array<std::unique_ptr<Abstract::Derivative>> derivatives;

public:
    /// Adds derivative if not already present. If the derivative is already stored, new one is NOT
    /// created, even if settings are different.
    template <typename TDerivative, typename... TArgs>
    void require(TArgs&&... args) {
        for (auto& d : derivatives) {
            if (typeid(d.get()) == typeid(TDerivative*)) {
                return;
            }
        }
        derivatives.push(std::make_unique<TDerivative>(std::forward<TArgs>(args)...));
    }

    /// Initialize derivatives before loop
    void initialize(const Storage& input) {
        accumulated.initialize(input.getParticleCnt());
        for (auto& deriv : derivatives) {
            deriv->initialize(input, accumulated);
        }
    }
};

NAMESPACE_SPH_END
