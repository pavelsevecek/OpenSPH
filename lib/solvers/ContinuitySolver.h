#pragma once

/// Standard SPH solver, using density and specific energy as independent variables. Evolves density using
/// continuity equation and energy using energy equation. Works with any artificial viscosity and any equation
/// of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "solvers/AbstractSolver.h"
#include "solvers/Accumulator.h"
#include "sph/av/Standard.h"
#include "sph/boundary/Boundary.h"
#include "sph/distributions/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "storage/Iterate.h"
#include "storage/QuantityKey.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Settings.h"


NAMESPACE_SPH_BEGIN

template <typename TContext>
class ContinuitySolver : public SolverBase<TContext> {
private:
    TContext context;
    static constexpr int dim = TContext::dim;

    Accumulator<RhoDivv> rhoDivv;

    /// Map of units required by this solver. Additional units are needed by solver context; if some units are
    /// shared, their type and order must match!
    std::map<QuantityKey, Tuple<ValueEnum, OrderEnum>> map{
        { QuantityKey::RHO, { ValueEnum::SCALAR, OrderEnum::FIRST_ORDER } },
        { QuantityKey::M, { ValueEnum::SCALAR, OrderEnum::ZERO_ORDER } },
    };


public:
    ContinuitySolver(const Settings<GlobalSettingsIds>& settings)
        : SolverBase<TContext>(settings)
        , context(settings) {}

    virtual void compute(Storage& storage) override {
        const int size = storage.getParticleCnt();

        ArrayView<Vector> r, v, dv;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        ArrayView<Float> m, rho, drho;
        tieToArray(rho, drho) = storage.getAll<Float>(QuantityKey::RHO);
        m = storage.getValue<Float>(QuantityKey::M);
        // Check that quantities are valid
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
        ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));

        // clamp smoothing length
        for (Float& h : componentAdapter(r, H)) {
            h = Math::max(h, 1.e-12_f);
        }
        // initialize stuff
        rhoDivv.update(storage);
        context.update(storage);

        // find new pressure
        this->computeMaterial(storage);

        // build neighbour finding object
        /// \todo only rebuild(), try to limit allocations
        PROFILE_SCOPE("ContinuitySolver::compute (init)")
        this->finder->build(r);

        // we symmetrize kernel by averaging smoothing lenghts
        SymH<dim> w(this->kernel);

        PROFILE_NEXT("ContinuitySolver::compute (main cycle)")
        for (int i = 0; i < size; ++i) {
            // Find all neighbours within kernel support. Since we are only searching for particles with
            // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to
            // h_i, and we thus never "miss" a particle.
            this->finder->findNeighbours(
                i, r[i][H] * this->kernel.radius(), this->neighs, FinderFlags::FIND_ONLY_SMALLER_H);
            // iterate over neighbours
            for (const auto& neigh : this->neighs) {
                const int j = neigh.index;
                // actual smoothing length
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS && hbar <= r[i][H]);
                if (getSqrLength(r[i] - r[j]) > Math::sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

                const Vector f = context.dv(i, j, grad);
                ASSERT(Math::isReal(f));
                dv[i] += m[j] * f; // opposite sign due to antisymmetry of gradient
                dv[j] -= m[i] * f;

                rhoDivv.accumulate(i, j, grad);
                context.accumulate(i, j, grad);
            }
        }

        // set derivative of density and smoothing length
        for (int i = 0; i < drho.size(); ++i) {
            drho[i] = -rhoDivv[i];
            /// \todo smoothing length
            v[i][H] = 0._f;
            dv[i][H] = 0._f;
        }
        context.evaluate();
        if (this->boundary) {
            this->boundary->apply(storage);
        }
    }
};

NAMESPACE_SPH_END
