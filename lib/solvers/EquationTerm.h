#pragma once

#include "solvers/Derivative.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    /// Represents a term or terms appearing in evolutionary equations. Each EquationTerm either directly
    /// modifies quantities, or adds quantity derivatives. These terms never work directly with pairs of
    /// particles, instead they can register Abstract::Derivative that will be accumulated by the solver.
    /// EquationTerm can then access the result.
    class EquationTerm : public Polymorphic {
    public:
        virtual void setDerivatives(DerivativeHolder& storage) = 0;

        virtual void initialize() = 0;

        virtual void finalize(Storage& storage, Accumulated& accumulated) = 0;
    };
}

/// Computes acceleration from pressure gradient and corresponding increment of internal energy.
class PressureForce : public Abstract::EquationTerm {
private:
    class PressureGradient : public Abstract::Derivative {
    private:
        ArrayView<const Float> p, rho, m;
        ArrayView<Vector> dv;

    public:
        virtual void initialize(const Storage& input, Accumulated& results) override {
            tie(p, rho, m) =
                input.getValues<Float>(QuantityIds::PRESSURE, QuantityIds::DENSITY, QuantityIds::MASSES);
            dv = results.getValue<Vector>(QuantityIds::POSITIONS);
        }

        virtual void compute(const Size i,
            ArrayView<const NeighbourRecord> neighs,
            ArrayView<const Vector> grads) override {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k].index;
                const Vector f = (p[i] + p[j]) / (rho[i] * rho[j]) * grads[k];
                dv[i] += m[j] * f;
                dv[j] -= m[i] * f;
            }
        }
    };

public:
    // sets up the force for single thread, will be called multiple times
    virtual void setDerivatives(DerivativeHolder& derivatives) override {
        derivatives.require<PressureGradient>();
        derivatives.require<VelocityDivergence>();
    }

    virtual void finalize(Storage& storage, Accumulated& accumulated) override {
        storage.getD2t<Vector>(QuantityIds::POSITIONS) =
            std::move(accumulated.getValue<Vector>(QuantityIds::POSITIONS));
        ArrayView<const Float> divv = accumulated.getValue<Float>(QuantityIds::VELOCITY_DIVERGENCE);
        ArrayView<const Float> p, rho, m; ///\todo share these arrayviews?
        tie(p, rho, m) =
            storage.getValues<Float>(QuantityIds::PRESSURE, QuantityIds::DENSITY, QuantityIds::MASSES);
        ArrayView<Float> du = storage.getDt<Float>(QuantityIds::ENERGY);

        for (Size i = 0; i < du.size(); ++i) {
            du[i] += p[i] / rho[i] * divv[i];
        }
    }
};

NAMESPACE_SPH_END
