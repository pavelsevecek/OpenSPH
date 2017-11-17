#pragma once

/// \file Setup.h
/// \brief Helper functions performing common tasks in unit testing and benchmarks
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/geometry/Domain.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

namespace Tests {
    /// Creates a particle storage with positions, density and masses, filling sphere of radius 1.
    /// Particles have no material and the density is 1.
    Storage getStorage(const Size particleCnt);

    /// Returns a storage with ideal gas particles, having pressure, energy and sound speed.
    Storage getGassStorage(const Size particleCnt, BodySettings settings, const IDomain& domain);

    /// Returns a storage with ideal gas particles, filling a spherical domain of given radius.
    Storage getGassStorage(const Size particleCnt,
        BodySettings settings = BodySettings::getDefaults(),
        const Float radius = 1.f);

    /// Returns a storage with stress tensor.
    Storage getSolidStorage(const Size particleCnt, BodySettings settings, const IDomain& domain);

    /// Returns a storage with stress tensor.
    Storage getSolidStorage(const Size particleCnt,
        BodySettings settings = BodySettings::getDefaults(),
        const Float radius = 1._f);

    /// Returns the index to the particle closest to given point.
    Size getClosestParticle(const Storage& storage, const Vector& p);


    /// Computes velocity derivatives for given set of equations. Velocity field is defined by given
    /// lambda.
    template <typename TLambda>
    void computeField(Storage& storage,
        EquationHolder&& equations,
        TLambda&& lambda,
        const Size repeatCnt = 1) {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] = lambda(r[i]);
        }
        equations += makeTerm<ConstSmoothingLength>();
        GenericSolver solver(RunSettings::getDefaults(), std::move(equations));
        solver.create(storage, storage.getMaterial(0));
        Statistics stats;
        for (Size i = 0; i < repeatCnt; ++i) {
            solver.integrate(storage, stats);
        }
    }

    // Helper equation term using single derivative
    template <typename TDerivative>
    struct DerivativeWrapper : public IEquationTerm {
        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
            derivatives.require<TDerivative>(settings);
        }

        virtual void initialize(Storage& UNUSED(storage)) override {}

        virtual void finalize(Storage& UNUSED(storage)) override {}

        virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
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
