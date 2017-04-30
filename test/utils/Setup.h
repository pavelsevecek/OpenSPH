#pragma once

/// Helper functions performing common tasks in unit testings.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

namespace Tests {
    /// Creates a particle storage with positions, density and masses, filling sphere of radius 1.
    /// Particles have no material and the density is 1.
    Storage getStorage(const Size particleCnt);

    /// Returns a storage with ideal gas particles, having pressure, energy and sound speed.
    Storage getGassStorage(const Size particleCnt,
        BodySettings settings = BodySettings::getDefaults(),
        const Float radius = 1._f);

    /// Computes velocity derivatives for given set of equations. Velocity field is defined by given
    /// lambda.
    template <typename TLambda>
    void computeField(Storage& storage,
        EquationHolder&& equations,
        TLambda&& lambda,
        const Size repeatCnt = 1) {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] = lambda(r[i]);
        }
        GenericSolver solver(RunSettings::getDefaults(), std::move(equations));
        solver.create(storage, storage.getMaterial(0));
        Statistics stats;
        for (Size i = 0; i < repeatCnt; ++i) {
            solver.integrate(storage, stats);
        }
    }

    // Helper equation term using single derivative
    template <typename TDerivative>
    struct DerivativeWrapper : public Abstract::EquationTerm {
        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
            derivatives.require<TDerivative>(settings);
        }

        virtual void initialize(Storage& UNUSED(storage)) override {}

        virtual void finalize(Storage& UNUSED(storage)) override {}

        virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
    };

    /// Computes only a single derivative.
    template <typename TDerivative, typename TLambda>
    void computeField(Storage& storage, TLambda&& lambda, const Size repeatCnt = 1) {
        EquationHolder equations;
        equations += makeTerm<DerivativeWrapper<TDerivative>>();
        computeField(storage, std::move(equations), std::forward<TLambda>(lambda), repeatCnt);
    }
}

NAMESPACE_SPH_END
