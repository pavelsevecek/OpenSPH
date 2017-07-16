#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/BarnesHut.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Gravity solver
class GravitySolver : public GenericSolver {
private:
    BarnesHut gravity;

    /// Dummy term to make sure acceleration is being accumulated
    struct DummyAcceleration : public Abstract::EquationTerm {
        struct DummyDerivative : public Abstract::Derivative {
            virtual void create(Accumulated& results) override {
                results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            }
            virtual void initialize(const Storage&, Accumulated&) override {}
            virtual void evalSymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
            virtual void evalAsymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {
            }
        };

        virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
            derivatives.require<DummyDerivative>(settings);
        }
        virtual void initialize(Storage&) override {}
        virtual void finalize(Storage&) override {}
        virtual void create(Storage&, Abstract::Material&) const override {}
    };

public:
    GravitySolver(const RunSettings& settings, const EquationHolder& equations)
        : GenericSolver(settings, equations + makeTerm<DummyAcceleration>())
        , gravity(settings.get<Float>(RunSettingsId::GRAVITY_OPENING_ANGLE),
              MultipoleOrder::OCTUPOLE,
              Factory::getGravityKernel(settings)) {}

protected:
    virtual void loop(Storage& storage) override {
        // first, do asymmetric evaluation of gravity
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);

        // build Barnes-Hut tree
        gravity.build(r, m);

        // evaluate gravity for each particle
        auto functor = [this, r](const Size n1, const Size n2, ThreadData& data) {
            Accumulated& accumulated = data.derivatives.getAccumulated();
            ArrayView<Vector> dv = accumulated.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            for (Size i = n1; i < n2; ++i) {
                dv[i] += gravity.eval(i);
            }
        };
        parallelFor(*pool, threadData, 0, r.size(), granularity, functor);

        // second, compute SPH derivatives using symmetric evaluation
        GenericSolver::loop(storage);
    }
};

NAMESPACE_SPH_END
