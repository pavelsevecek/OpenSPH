#pragma once

/// Standard SPH solver, using density and specific energy as independent variables. Evolves density using
/// continuity equation and energy using energy equation. Works with any artificial viscosity and any equation
/// of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Means.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/AbstractFinder.h"
#include "quantities/Iterate.h"
#include "quantities/QuantityIds.h"
#include "solvers/AbstractSolver.h"
#include "sph/av/Standard.h"
#include "sph/boundary/Boundary.h"
#include "sph/forces/StressForce.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN
/*
template <typename Force, int D>
class ContinuitySolver : public SolverBase<D>, Module<Force, RhoDivv, RhoGradv> {
private:
    Force force;

    static constexpr int dim = D;

    /// \todo SharedAccumulator
    RhoDivv rhoDivv;

    /// \todo sharedaccumulator
    RhoGradv rhoGradv;

    Float minH;

    Range neighRange;

    ThreadPool pool;

public:
    ContinuitySolver(const GlobalSettings& settings)
        : SolverBase<D>(settings)
        , Module<Force, RhoDivv, RhoGradv>(force, rhoDivv, rhoGradv)
        , force(settings) {
        minH = settings.get<Float>(GlobalSettingsIds::SPH_SMOOTHING_LENGTH_MIN);
        neighRange = settings.get<Range>(GlobalSettingsIds::SPH_NEIGHBOUR_RANGE);
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        force.stats = &stats;
        ASSERT(storage.isValid());
        const Size size = storage.getParticleCnt();
        ArrayView<Vector> r, v, dv;
        ArrayView<Float> m, rho, drho;
        {
            PROFILE_SCOPE("ContinuitySolver::compute (getters)")
            tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
            tie(rho, drho) = storage.getAll<Float>(QuantityIds::DENSITY);
            m = storage.getValue<Float>(QuantityIds::MASSES);
            // Check that quantities are valid
            ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
            ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));

            // clamp smoothing length
            for (Float& h : componentAdapter(r, H)) {
                h = max(h, minH);
            }
        }
        Array<Size>& neighCnts = storage.getValue<Size>(QuantityIds::NEIGHBOUR_CNT);
        neighCnts.fill(0);

        {
            PROFILE_SCOPE("ContinuitySolver::compute (updateModules)")
            this->updateModules(storage);
        }
        {
            PROFILE_SCOPE("ContinuitySolver::compute (build)")
            this->finder->build(r);
        }
        // we symmetrize kernel by averaging smoothing lenghts
        SymH<dim> w(this->kernel);
        /// \todo for parallelization we need array of neighbours for each thread
        /// + figure out how to do parallelization correctly, hm ...
        for (Size i = 0; i < size; ++i) {
            // Find all neighbours within kernel support. Since we are only searching for particles with
            // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to
            // h_i, and we thus never "miss" a particle.
            const Size neighCnt = this->finder->findNeighbours(
                i, r[i][H] * this->kernel.radius(), this->neighs, FinderFlags::FIND_ONLY_SMALLER_H);
            neighCnts[i] += neighCnt;
            // iterate over neighbours
            PROFILE_SCOPE("ContinuitySolver::compute (iterate)")
            for (const auto& neigh : this->neighs) {
                const Size j = neigh.index;
                neighCnts[j]++;
                // actual smoothing length
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS && hbar <= r[i][H]);
                if (getSqrLength(r[i] - r[j]) >= sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(isReal(grad) && dot(grad, r[i] - r[j]) < 0._f);

                this->accumulateModules(i, j, grad);
            }
        }

        // set derivative of density and smoothing length
        ArrayView<TracelessTensor> s;
        if (storage.has(QuantityIds::DEVIATORIC_STRESS)) {
            s = storage.getValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        }
        ArrayView<Float> dmg;
        if (storage.has(QuantityIds::DAMAGE)) {
            dmg = storage.getValue<Float>(QuantityIds::DAMAGE);
        }
        ArrayView<Float> reducing;
        if (storage.has(QuantityIds::YIELDING_REDUCE)) {
            reducing = storage.getValue<Float>(QuantityIds::YIELDING_REDUCE);
        }
        Float divv_max = 0._f;
        for (Size i = 0; i < size; ++i) {
            divv_max = max(divv_max, rhoDivv[i]);
        }
        ArrayView<Size> flag = storage.getValue<Size>(QuantityIds::FLAG);
        for (Size i = 0; i < drho.size(); ++i) {
            Float divv;
            const Float red = dmg ? 1.f - pow<3>(dmg[i]) : 1.f;
            if (s && red * reducing[i] != 0._f) {
                // nonzero stress tensor
                divv = rhoGradv[i].trace();
            } else {
                divv = rhoDivv[i];
            }
            drho[i] = -rho[i] * divv;

            if (flag[i] == 0) {
                // undamaged target
                v[i][H] = r[i][H] / D * divv;
            } else {
                // damaged target or impactor
                v[i][H] = r[i][H] / D * rhoDivv[i];
            }

            dv[i][H] = 0._f;
        }
        this->integrateModules(storage);
        Means neighMeans;
        for (Size i = 0; i < size; ++i) {
            neighMeans.accumulate(neighCnts[i]);
        }
        stats.set(StatisticsIds::NEIGHBOUR_COUNT, neighMeans);
        if (this->boundary) {
            PROFILE_SCOPE("ContinuitySolver::compute (boundary)")
            this->boundary->apply(storage);
        }
    }

    virtual void initialize(Storage& storage, const BodySettings& settings) const override {
        storage.insert<Float, OrderEnum::FIRST>(QuantityIds::DENSITY,
            settings.get<Float>(BodySettingsIds::DENSITY),
            settings.get<Range>(BodySettingsIds::DENSITY_RANGE));
        MaterialAccessor(storage).minimal(QuantityIds::DENSITY, 0) =
            settings.get<Float>(BodySettingsIds::DENSITY_MIN);
        storage.insert<Size, OrderEnum::ZERO>(QuantityIds::NEIGHBOUR_CNT, 0);
        this->initializeModules(storage, settings);
    }
};*/

NAMESPACE_SPH_END
