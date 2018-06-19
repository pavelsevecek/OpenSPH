#pragma once

#include "objects/finders/NeighbourFinder.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// See Owen 2009: A compatibly differenced total energy conserving form of SPH
class DifferencedEnergySolver : public AsymmetricSolver {
private:
    struct {
        DerivativeHolder derivatives;
        Storage storage;
    } accelerations;

    Float initialDt;


public:
    DifferencedEnergySolver(const RunSettings& settings, const EquationHolder& eqs)
        : AsymmetricSolver(settings, eqs) {
        /// \todo accelerations.derivatives = derivatives.getSubset(QuantityId::POSITION, OrderEnum::SECOND);
        initialDt = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        AsymmetricSolver::integrate(storage, stats);

        ArrayView<const Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        /// \todo maybe simply pass the timestep into the function?
        const Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, initialDt);

        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);

        Accumulated& accumulated = accelerations.derivatives.getAccumulated();
        /// \todo accumulated.clear();
        accelerations.derivatives.initialize(storage);
        ArrayView<Vector> dvij = accumulated.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);

        // we need to symmetrize kernel in smoothing lenghts to conserve momentum
        SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> symmetrizedKernel(kernel);

        Float maxH = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            maxH = max(maxH, r[i][H]);
        }
        const Float radius = 1.01_f * maxH * kernel.radius();

        auto functor = [&](const Size i, ThreadData& data) {
            finder->findAll(i, radius, data.neighs);
            du[i] = 0._f;
            for (auto& n : data.neighs) {
                const Size j = n.index;
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS, hbar);
                if (i == j || getSqrLength(r[i] - r[j]) >= sqr(kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                const Vector gr = symmetrizedKernel.grad(r[i], r[j]);
                ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) < 0._f, gr, r[i] - r[j]);
                dvij[i] = Vector(0._f);
                accelerations.derivatives.eval(i, getSingleValueView(j), getSingleValueView(gr));

                const Vector vi12 = v[i] + 0.5_f * dv[i] * dt;
                const Vector vj12 = v[j] + 0.5_f * dv[j] * dt;
                du[i] += 0.5_f * dot(vj12 - vi12, dvij[i]);
            }
        };
        PROFILE_SCOPE("AsymmetricSolver main loop");
        parallelFor(pool, threadData, 0, r.size(), granularity, functor);
    }
};

NAMESPACE_SPH_END
