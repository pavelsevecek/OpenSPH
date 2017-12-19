#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/IGravity.h"
#include "sph/equations/Potentials.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Gravity solver
class GravitySolver : public GenericSolver {
private:
    /// Implementation of gravity used by the solver
    AutoPtr<IGravity> gravity;

    struct DummyDerivative : public IDerivative {
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        }
        virtual void initialize(const Storage&, Accumulated&) override {}
        virtual void evalSymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
        virtual void evalAsymmetric(const Size, ArrayView<const Size>, ArrayView<const Vector>) override {}
    };

public:
    GravitySolver(const RunSettings& settings, const EquationHolder& equations, AutoPtr<IGravity>&& gravity)
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

        // get acceleration buffer corresponding to first thread (to save some memory + time)
        ThreadData& data = threadData.first();
        Accumulated& accumulated = data.derivatives.getAccumulated();
        ArrayView<Vector> dv = accumulated.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);

        // evaluate gravity for each particle
        MEASURE("Evaluating gravity", gravity->evalAll(*pool, dv, stats));

        // second, compute SPH derivatives using symmetric evaluation
        MEASURE("Evaluating SPH", GenericSolver::loop(storage, stats));
    }
};

NAMESPACE_SPH_END
