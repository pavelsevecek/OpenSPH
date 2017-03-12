#pragma once

#include "solvers/Derivative.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    /// Represents a term or terms appearing in evolutionary equations.
    class EquationTerm : public Polymorphic {
    public:
    };
}

/// Computes acceleration from pressure gradient and corresponding increment of internal energy.
class PressureForce : public Abstract::EquationTerm {
private:
public:
    // sets up the force for single thread, will be called multiple times
    virtual void initializeThread(DerivativeHolder& derivatives) override {
        derivatives.require<PressureGradient>();
        derivatives.require<VelocityDivergence>();
    }

    void integrate() {
        e = p[i] / rho[i] * divv[i];
        du[i] += m[i] * e;
        du[j] += m[j] * e;
    }
};

NAMESPACE_SPH_END
