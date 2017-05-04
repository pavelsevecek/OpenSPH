#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "physics/Constants.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Gravity solver
///
/// Currently brute-force.
class GravitySolver : public GenericSolver {
private:
    SymmetrizeValues<GravityLutKernel> gravityKernel;

    /// Dummy term to make sure acceleration is being accumulated
    struct DummyAcceleration : public Abstract::EquationTerm {
        struct DummyDerivative : public Abstract::Derivative {
            virtual void create(Accumulated& results) {
                results.insert<Vector>(QuantityId::POSITIONS);
            }
            virtual void initialize(const Storage&, Accumulated&) override {}
            virtual void compute(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
        };

        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
            derivatives.require<DummyDerivative>(settings);
        }
        virtual void initialize(Storage&) override {}
        virtual void finalize(Storage&) override {}
        virtual void create(Storage&, Abstract::Material&) const override {}
    };

public:
    GravitySolver(const RunSettings& settings, EquationHolder&& equations)
        : GenericSolver(settings, std::move(equations += makeTerm<DummyAcceleration>())) {
        gravityKernel = Factory::getGravityKernel(settings);
    }

protected:
    virtual void loop(Storage& storage) override {
        // brute force solution, evaluate every pair of particles
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);
        auto functor = [this, r, m](const Size n1, const Size n2, ThreadData& data) {
            /// \todo avoid getting accumulated storage here
            Accumulated& accumulated = data.derivatives.getAccumulated();
            ArrayView<Vector> dv = accumulated.getValue<Vector>(QuantityId::POSITIONS);
            const Float G = Constants::gravity;
            for (Size i = n1; i < n2; ++i) {
                data.grads.clear();
                data.idxs.clear();
                for (Size j = 0; j < i; ++j) {
                    const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                    ASSERT(hbar > EPS && hbar <= r[i][H]);
                    if (getSqrLength(r[i] - r[j]) < sqr(this->kernel.radius() * hbar)) {
                        // inside the support of SPH kernel, save for evaluating SPH derivatives
                        const Vector gr = kernel.grad(r[i], r[j]);
                        ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) < 0._f);
                        data.grads.emplaceBack(gr);
                        data.idxs.emplaceBack(j);
                    }
                    // evaluate gravity for EVERY pair of particles
                    const Vector gr = gravityKernel.grad(r[i], r[j]);
                    // factor 1/2 in Eq. (3.129) of Cossins 2010 is already included in symmetrized kernel
                    dv[i] -= G * m[j] * gr;
                    dv[j] += G * m[i] * gr;
                }
                data.derivatives.compute(i, data.idxs, data.grads);
            }
        };
        parallelFor(*pool, threadData, 0, r.size(), granularity, functor);
    }
};

NAMESPACE_SPH_END
