#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/AbstractGravity.h"
#include "sph/equations/Potentials.h"
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
        // check the equations
        this->sanityCheck();
        // add acceleration as it is needed by gravity
        threadData.forEach([&settings](ThreadData& data) { //
            data.derivatives.require<DummyDerivative>(settings);
        });
    }

protected:
    virtual void loop(Storage& storage) override {
        // first, do asymmetric evaluation of gravity
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);

        // build gravity tree
        PROFILE("Building gravity", gravity->build(storage));

        // evaluate gravity for each particle
        auto functor = [this, r](const Size n1, const Size n2, ThreadData& data) {
            Accumulated& accumulated = data.derivatives.getAccumulated();
            ArrayView<Vector> dv = accumulated.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            Statistics dummy;
            for (Size i = n1; i < n2; ++i) {
                /// \todo refactor
                dv[i] += gravity->eval(r[i], dummy);
            }
        };
        PROFILE("Evaluating gravity", parallelFor(*pool, threadData, 0, r.size(), granularity, functor));

        // second, compute SPH derivatives using symmetric evaluation
        PROFILE("Evaluating SPH", GenericSolver::loop(storage));
    }

    void sanityCheck() const {
        // check that we don't solve gravity twice
        /// \todo generalize for ALL solvers of gravity (some categories?)
        if (equations.contains<SphericalGravity>()) {
            throw InvalidSetup(
                "Cannot use SphericalGravity in GravitySolver; only one solver of gravity is allowed");
        }
    }
};

NAMESPACE_SPH_END
