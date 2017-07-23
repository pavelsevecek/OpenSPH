#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/AbstractGravity.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Gravity solver
class GravitySolver : public GenericSolver {
private:
    AutoPtr<Abstract::Gravity> gravity;

    struct DummyDerivative : public Abstract::Derivative {
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
        }
        virtual void initialize(const Storage&, Accumulated&) override {}
        virtual void evalSymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
        virtual void evalAsymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
    };

public:
    GravitySolver(const RunSettings& settings,
        const EquationHolder& equations,
        AutoPtr<Abstract::Gravity>&& gravity)
        : GenericSolver(settings, equations)
        , gravity(std::move(gravity)) {
        // add acceleration as it is needed by gravity
        threadData.forEach([&settings](ThreadData& data) { //
            data.derivatives.require<DummyDerivative>(settings);
        });
    }

protected:
    virtual void loop(Storage& storage) override {
        // first, do asymmetric evaluation of gravity
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);

        // build gravity tree
        {
            ScopedTimer __timer("", [](const std::string&, const uint64_t time) {
                StdOutLogger logger;
                logger.write("Building took ", time / 1000, " ms");
            });
            gravity->build(r, m);
        }

        // evaluate gravity for each particle
        auto functor = [this, r](const Size n1, const Size n2, ThreadData& data) {
            Accumulated& accumulated = data.derivatives.getAccumulated();
            ArrayView<Vector> dv = accumulated.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            Statistics dummy;
            for (Size i = n1; i < n2; ++i) {
                dv[i] += gravity->eval(i, dummy);
            }
        };
        {
            ScopedTimer __timer("", [](const std::string&, const uint64_t time) {
                StdOutLogger logger;
                logger.write("Evaluating gravity took ", time / 1000, " ms");
            });
            parallelFor(*pool, threadData, 0, r.size(), granularity, functor);
        }

        {
            ScopedTimer __timer("", [](const std::string&, const uint64_t time) {
                StdOutLogger logger;
                logger.write("Evaluating SPH ", time / 1000, " ms");
            });
            // second, compute SPH derivatives using symmetric evaluation
            GenericSolver::loop(storage);
        }
    }
};

NAMESPACE_SPH_END
