#pragma once

/// \file DensityIndependentSolver.h
/// \brief Density-independent formulation of SPH
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz
/// \date 2016-2018

#include "sph/equations/EquationTerm.h"
#include "sph/equations/av/Standard.h"
#include "sph/kernel/KernelFactory.h"
#include "sph/solvers/SymmetricSolver.h"

NAMESPACE_SPH_BEGIN

class DensityIndependentPressureForce : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<const Float> q, U, m;
        ArrayView<const Vector> r, v;
        ArrayView<Vector> dv;
        ArrayView<Float> dU;
        Float gamma;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Float>(QuantityId::ENERGY_PER_PARTICLE, OrderEnum::FIRST);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            tie(m, q, U) = input.getValues<Float>(
                QuantityId::MASS, QuantityId::ENERGY_DENSITY, QuantityId::ENERGY_PER_PARTICLE);
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            dU = results.getBuffer<Float>(QuantityId::ENERGY_PER_PARTICLE, OrderEnum::FIRST);
            /// \todo here we assume all particles have the same adiabatic index, as in Saitoh & Makino.
            /// DISPH would need to be generalized for particles with different gamma.
            gamma = input.getMaterial(0)->getParam<Float>(BodySettingsId::ADIABATIC_INDEX);
            ASSERT(gamma > 1._f);
        }

        template <bool Symmetric>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Vector f = (gamma - 1._f) * U[i] * U[j] * (1._f / q[i] + 1._f / q[j]) * grad;
            ASSERT(isReal(f));
            dv[i] -= f / m[i];
            /// \todo possible optimization, acceleration could be multiplied by factor (gamma-1)/m_i
            /// could be after the loop
            const Float e = (gamma - 1._f) * U[i] * U[j] * dot(v[i] - v[j], grad);
            dU[i] += e / q[i];

            if (Symmetric) {
                dv[j] += f / m[j];
                dU[j] += e / q[j];
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& material) const override {
        // energy density = specific energy * density
        const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
        const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
        const Float q0 = rho0 * u0;
        /// \todo replace asserts with exceptions for incorrect setup of solver!!!
        if (q0 <= 0._f) {
            throw InvalidSetup("Cannot use DISPH with zero specific energy");
        }

        const Interval rhoRange = material.getParam<Interval>(BodySettingsId::DENSITY_RANGE);
        const Interval uRange = material.getParam<Interval>(BodySettingsId::ENERGY_RANGE);
        const Interval qRange(rhoRange.lower() * uRange.lower(), rhoRange.upper() * uRange.upper());
        if (qRange.lower() <= 0._f) {
            throw InvalidSetup("Cannot use DISPH with zero specific energy");
        }
        const Float rhoMin = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
        const Float uMin = material.getParam<Float>(BodySettingsId::ENERGY_MIN);
        const Float qMin = rhoMin * uMin;

        material.setRange(QuantityId::ENERGY_DENSITY, qRange, qMin);

        // energy density is computed by direct sum, hence zero order
        storage.insert<Float>(QuantityId::ENERGY_DENSITY, OrderEnum::ZERO, q0);

        // energy per particle
        Array<Float> e = storage.getValue<Float>(QuantityId::MASS).clone();
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
        ASSERT(dynamic_cast<const IdealGasEos*>(&eos.getEos()));
        Float p0, cs0;
        tie(p0, cs0) = eos.evaluate(rho0, u0);
        storage.insert<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO, cs0);
        storage.insert<Float>(QuantityId::PRESSURE, OrderEnum::ZERO, p0);
    }
};


/// \brief Density-independent SPH solver
///
/// Uses solver of Saitoh & Makino \cite Saitoh_Makino_20133. Instead of density and specific energy,
/// independent variables are energy density (q) and internal energy of i-th particle (U). Otherwise, the
/// solver is similar to \ref SummationSolver; the energy density is computed using direct summation by
/// self-consistent solution with smoothing length.
///
/// \attention Works only for ideal gas EoS!
class DensityIndependentSolver : public SymmetricSolver {
private:
    LutKernel<DIMENSIONS> energyKernel;
    Array<Float> q;

public:
    explicit DensityIndependentSolver(const RunSettings& settings)
        : SymmetricSolver(settings, getEquations(settings)) {
        energyKernel = Factory::getKernel<DIMENSIONS>(settings);
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
        storage.insert<Float>(
            QuantityId::DENSITY, OrderEnum::ZERO, material.getParam<Float>(BodySettingsId::DENSITY));
        material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);
        equations.create(storage, material);
    }

private:
    static EquationHolder getEquations(const RunSettings& UNUSED(settings)) {
        EquationHolder equations;
        equations += makeTerm<DensityIndependentPressureForce>() + makeTerm<StandardAV>() +
                     makeTerm<ConstSmoothingLength>();

        return equations;
    }

    virtual void beforeLoop(Storage& storage, Statistics& stats) override {
        SymmetricSolver::beforeLoop(storage, stats);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Float> U = storage.getValue<Float>(QuantityId::ENERGY_PER_PARTICLE);

        q.resize(r.size());
        q.fill(EPS);
        auto functor = [this, r, U](const Size n1, const Size n2, ThreadData& data) {
            for (Size i = n1; i < n2; ++i) {
                // find all neighbours
                finder->findAll(i, r[i][H] * kernel.radius(), data.neighs);
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
        parallelFor(*pool, threadData, 0, r.size(), granularity, functor);

        // save computed values
        std::swap(storage.getValue<Float>(QuantityId::ENERGY_DENSITY), q);
    }

    virtual void afterLoop(Storage& storage, Statistics& statistics) override {
        SymmetricSolver::afterLoop(storage, statistics);
        // compute dependent quantities
        ArrayView<Float> q, U, rho, m, u;
        tie(q, U, rho, m, u) = storage.getValues<Float>(QuantityId::ENERGY_DENSITY,
            QuantityId::ENERGY_PER_PARTICLE,
            QuantityId::DENSITY,
            QuantityId::MASS,
            QuantityId::ENERGY);
        for (Size i = 0; i < u.size(); ++i) {
            u[i] = U[i] / m[i];
            ASSERT(u[i] > 0._f);
            rho[i] = q[i] / u[i];
        }
    }
};

NAMESPACE_SPH_END
