#pragma once

#include "solvers/AbstractSolver.h"
#include "solvers/Accumulated.h"
#include "solvers/EquationTerm.h"
#include "sph/Material.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

/// Computes derivatives
class GenericSolver : public Abstract::Solver {
protected:
    struct ThreadData {
        DerivativeHolder derivatives;

        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Indices of real neighbours
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    ThreadPool pool;

    Size granularity;

    /// Thread-local data
    ThreadLocal<ThreadData> threadData;

    EquationHolder equations;

    std::unique_ptr<Abstract::Finder> finder;

    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> kernel;

public:
    GenericSolver(const RunSettings& settings, EquationHolder&& eqs)
        : pool(settings.get<int>(RunSettingsId::RUN_THREAD_CNT))
        , threadData(pool) {
        kernel = Factory::getKernel<DIMENSIONS>(settings);
        finder = Factory::getFinder(settings);
        granularity = settings.get<int>(RunSettingsId::RUN_THREAD_GRANULARITY);
        equations += std::move(eqs);
        // add term counting number of neighbours
        equations += makeTerm<NeighbourCountTerm>();
        // initialize all derivatives
        threadData.forEach([this](ThreadData& data) { equations.setupThread(data.derivatives); });
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);

        // (re)build neighbour-finding structure
        finder->build(r);

        // initialize all materials (compute pressure, apply yielding and damage, ...)
        for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
            MaterialView material = storage.getMaterial(i);
            material->initialize(storage, material.sequence());
        }

        // initialize accumulate storages & derivatives
        this->beforeLoop(storage);

        // main loop over pairs of interacting particles
        auto functor = [this, r](const Size n1, const Size n2, ThreadData& data) {
            for (Size i = n1; i < n2; ++i) {
                // Find all neighbours within kernel support. Since we are only searching for particles with
                // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal
                // to h_i, and we thus never "miss" a particle.
                const FinderFlags flags = FinderFlags::FIND_ONLY_SMALLER_H;
                finder->findNeighbours(i, r[i][H] * kernel.radius(), data.neighs, flags);
                data.grads.clear();
                data.idxs.clear();
                for (auto& n : data.neighs) {
                    const Size j = n.index;
                    const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                    ASSERT(hbar > EPS && hbar <= r[i][H]);
                    if (getSqrLength(r[i] - r[j]) >= sqr(this->kernel.radius() * hbar)) {
                        // aren't actual neighbours
                        continue;
                    }
                    const Vector gr = kernel.grad(r[i], r[j]);
                    ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) < 0._f);
                    data.grads.emplaceBack(gr);
                    data.idxs.emplaceBack(j);
                }
                data.derivatives.compute(i, data.idxs, data.grads);
            }
        };
        parallelFor(pool, threadData, 0, r.size(), granularity, functor);

        // sum up accumulated storage, compute statistics
        this->afterLoop(storage, stats);

        // integrate all equations
        equations.finalize(storage);

        // finalize all materials (integrate fragmentation model)
        for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
            MaterialView material = storage.getMaterial(i);
            material->finalize(storage, material.sequence());
        }
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
        equations.create(storage, material);
    }

protected:
    virtual void beforeLoop(Storage& storage) {
        // clear thread local storages
        threadData.forEach([&storage](ThreadData& data) { data.derivatives.initialize(storage); });
    }

    virtual void afterLoop(Storage& storage, Statistics& stats) {
        // sum up thread local accumulated values
        Accumulated* first = nullptr;
        threadData.forEach([this, &first](ThreadData& data) {
            if (!first) {
                first = &data.derivatives.getAccumulated();
            } else {
                ASSERT(first != nullptr);
                first->sum(pool, data.derivatives.getAccumulated());
            }
        });
        ASSERT(first);
        // store them to storage
        first->store(storage);

        // compute neighbour statistics
        Means neighs;
        ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
        const Size size = storage.getParticleCnt();
        for (Size i = 0; i < size; ++i) {
            neighs.accumulate(neighCnts[i]);
        }
        stats.set(StatisticsId::NEIGHBOUR_COUNT, neighs);

        // Apply boundary conditions
        /// \todo add boundary equation as equation term
        boundary->apply(storage);
    }
};

NAMESPACE_SPH_END
