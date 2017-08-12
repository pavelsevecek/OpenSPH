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
    /// Implementation of gravity used by the solver
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
    virtual void loop(Storage& storage, Statistics& stats) override {
        // first, do asymmetric evaluation of gravity:

        // build gravity tree
        MEASURE("Building gravity", gravity->build(storage));

        // initialize thread-local acceleration arrayviews (needed by gravity),
        // we don't have to sum up the results, this is done by GenericSolver
        auto converter = [](ThreadData& data) -> ArrayView<Vector> {
            Accumulated& accumulated = data.derivatives.getAccumulated();
            return accumulated.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
        };
        ThreadLocal<ArrayView<Vector>> dv = threadData.convert<ArrayView<Vector>>(converter);

        // evaluate gravity for each particle
        MEASURE("Evaluating gravity", gravity->evalAll(*pool, dv, stats));

        // second, compute SPH derivatives using symmetric evaluation
        MEASURE("Evaluating SPH", GenericSolver::loop(storage, stats));
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
