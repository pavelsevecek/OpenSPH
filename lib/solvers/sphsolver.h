#pragma once

#include "solvers/AbstractSolver.h"
#include "solvers/PressureForce.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

class SphSolver : public Abstract::Solver {
private:
    struct ThreadData {
        Storage accumulated;
        DerivativeHolder derivatives;
        Array<NeighbourRecord> neighs;
        Array<Vector> grads;
    };

    ThreadLocal<ThreadData> storage;

    Array<Abstract::Solver> solvers;

    ThreadPool pool;

    std::unique_ptr<Abstract::Finder> finder;

public:
    virtual void initialize(Storage& storage, const GlobalSettings& settings) const {
        storage.forEach([this](ThreadData& data) { solvers.initializeThread(data.derivatives) })
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
        finder->build(r);

        parallelFor(pool, storage, 0, r.size(), [&](const Size n1, const Size n2, ThreadData& data) {
            for (Size i = n1; i < n2; ++i) {
                finder->findNeighbours(i, r[i][H] * kernel, data.neighs);
                data.grads.clear();
                for (auto& n : data.neighs) {
                    data.push(kernel.grad(i, j));
                }
                for (Derivative& d : data.derivatives) {
                    d.sum(i, neighs, grads);
                }
            }
        });
    }
};

NAMESPACE_SPH_END
