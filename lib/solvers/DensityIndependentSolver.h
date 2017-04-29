#pragma once

#include "solvers/EquationTerm.h"
#include "solvers/GenericSolver.h"
#include "sph/av/Standard.h"

NAMESPACE_SPH_BEGIN

/// Uses solver of Saitoh & Makino (2013). Instead of density and specific energy, independent variables are
/// energy density (q) and internal energy of i-th particle (U). Otherwise, the solver is similar to
/// SummationSolver; the energy density is computed using direct summation by self-consistent solution with
/// smoothing length. Works only for ideal gas EoS!

class DensityIndependentPressureForce : public Abstract::EquationTerm {
private:
    class Derivative : public Abstract::Derivative {
    private:
        ArrayView<const Size> matIdxs;
        ArrayView<const Float> q, U, m;
        ArrayView<const Vector> r, v;
        ArrayView<Vector> dv;
        ArrayView<Float> dU;
        Float gamma;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Float>(QuantityId::ENERGY_PER_PARTICLE);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            matIdxs = input.getValue<Size>(QuantityId::MATERIAL_IDX);
            tie(m, q, U) = input.getValues<Float>(
                QuantityId::MASSES, QuantityId::ENERGY_DENSITY, QuantityId::ENERGY_PER_PARTICLE);
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITIONS);

            dv = results.getValue<Vector>(QuantityId::POSITIONS);
            dU = results.getValue<Float>(QuantityId::ENERGY_PER_PARTICLE);
            /// \todo here we assume all particles have the same adiabatic index, as in Saitoh & Makino.
            /// DISPH would need to be generalized for particles with different gamma.
            gamma = input.getMaterial(0)->getParam<Float>(BodySettingsId::ADIABATIC_INDEX);
            ASSERT(gamma > 1._f);
        }

        virtual void compute(const Size i,
            ArrayView<const Size> neighs,
            ArrayView<const Vector> grads) override {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Vector f = (gamma - 1._f) * U[i] * U[j] * (1._f / q[i] + 1._f / q[j]) * grads[k];
                ASSERT(isReal(f));
                dv[i] -= f / m[i];
                dv[j] += f / m[j];
                /// \todo possible optimization, acceleration could be multiplied by factor (gamma-1)/m_i
                /// could be after the loop

                const Float e = (gamma - 1._f) * U[i] * U[j] * dot(v[i] - v[k], grads[k]);
                dU[i] += e / q[i];
                dU[j] += e / q[j];
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives) override {
        derivatives.require<Derivative>();
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        // energy density = specific energy * density
        const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
        const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
        const Float q0 = rho0 * u0;
        /// \todo replace asserts with exceptions for incorrect setup of solver!!!
        if (q0 <= 0._f) {
            throw InvalidSetup("Cannot use DISPH with zero specific energy");
        }

        const Range rhoRange = material.getParam<Range>(BodySettingsId::DENSITY_RANGE);
        const Range uRange = material.getParam<Range>(BodySettingsId::ENERGY_RANGE);
        const Range qRange(rhoRange.lower() * uRange.lower(), rhoRange.upper() * uRange.upper());
        if (qRange.lower() <= 0._f) {
            throw InvalidSetup("Cannot use DISPH with zero specific energy");
        }
        material.range(QuantityId::ENERGY_DENSITY) = qRange;

        const Float rhoMin = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
        const Float uMin = material.getParam<Float>(BodySettingsId::ENERGY_MIN);
        const Float qMin = rhoMin * uMin;
        material.minimal(QuantityId::ENERGY_DENSITY) = qMin;

        // energy density is computed by direct sum, hence zero order
        storage.insert<Float>(QuantityId::ENERGY_DENSITY, OrderEnum::ZERO, q0);

        // energy per particle
        Array<Float> e = storage.getValue<Float>(QuantityId::MASSES).clone();
        for (Size i = 0; i < e.size(); ++i) {
            e[i] *= u0;
        }
        /// \todo range and min value for energy per particle
        storage.insert<Float>(QuantityId::ENERGY_PER_PARTICLE, OrderEnum::FIRST, std::move(e));

        // Setup quantities with straighforward physical representation, used to output results of the solver
        // and for comparison with other solvers. Internal quantities of the solvers (energy per particle,
        // etc.) are always converted to the 'common' quantities after the loop.
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::ZERO, rho0);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::ZERO, u0);

        EosMaterial& eos = dynamic_cast<EosMaterial&>(material);
        Float p0, cs0;
        tie(p0, cs0) = eos.evaluate(rho0, u0);
        storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, cs0);
        storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, p0);
    }
};

class DensityIndependentSolver : public GenericSolver {
private:
    LutKernel<DIMENSIONS> energyKernel;
    Array<Float> q;

public:
    DensityIndependentSolver(const RunSettings& settings)
        : GenericSolver(settings, getEquations(settings)) {
        energyKernel = Factory::getKernel<DIMENSIONS>(settings);
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
        storage.insert<Float>(
            QuantityId::DENSITY, OrderEnum::ZERO, material.getParam<Float>(BodySettingsId::DENSITY));
        material.minimal(QuantityId::DENSITY) = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
        material.range(QuantityId::DENSITY) = material.getParam<Range>(BodySettingsId::DENSITY_RANGE);
        equations.create(storage, material);
    }

private:
    EquationHolder getEquations(const RunSettings& settings) {
        EquationHolder equations;
        equations += makeTerm<DensityIndependentPressureForce>();
        equations += makeTerm<StandardAV>(settings);

        return equations;
    }

    virtual void beforeLoop(Storage& storage, Statistics& stats) override {
        GenericSolver::beforeLoop(storage, stats);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<Float> U = storage.getValue<Float>(QuantityId::ENERGY_PER_PARTICLE);

        q.resize(r.size());
        q.fill(EPS);
        auto functor = [this, r, U](const Size n1, const Size n2, ThreadData& data) {
            for (Size i = n1; i < n2; ++i) {
                // find all neighbours
                finder->findNeighbours(i, r[i][H] * kernel.radius(), data.neighs, EMPTY_FLAGS);
                q[i] = 0._f;
                for (auto& n : data.neighs) {
                    const int j = n.index;
                    /// \todo can this be generally different kernel than the one used for derivatives?
                    q[i] += U[j] * energyKernel.value(r[i] - r[j], r[i][H]);
                }
            }
        };
        finder->build(r);
        /// \todo this should also be self-consistently solved with smoothing length (as SummationSolver)
        parallelFor(pool, threadData, 0, r.size(), granularity, functor);

        // save computed values
        std::swap(storage.getValue<Float>(QuantityId::ENERGY_DENSITY), q);
    }

    virtual void afterLoop(Storage& storage, Statistics& statistics) override {
        GenericSolver::afterLoop(storage, statistics);
        // compute dependent quantities
        ArrayView<Float> q, U, rho, m, u;
        tie(q, U, rho, m, u) = storage.getValues<Float>(QuantityId::ENERGY_DENSITY,
            QuantityId::ENERGY_PER_PARTICLE,
            QuantityId::DENSITY,
            QuantityId::MASSES,
            QuantityId::ENERGY);
        for (Size i = 0; i < u.size(); ++i) {
            u[i] = U[i] / m[i];
            ASSERT(u[i] > 0._f);
            rho[i] = q[i] / u[i];
        }
    }
};

NAMESPACE_SPH_END
