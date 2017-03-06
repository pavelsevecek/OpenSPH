#pragma once

#include "quantities/AbstractMaterial.h"
#include "solvers/AbstractSolver.h"
#include "solvers/Accumulator.h"

NAMESPACE_SPH_BEGIN

/// Uses solver of Saitoh & Makino (2013). Instead of density and specific energy, independent variables are
/// energy density (q) and internal energy of i-th particle (U). Otherwise, the solver is similar to
/// SummationSolver; the energy density is computed using direct summation by self-consistent solution with
/// smoothing length. Works only for ideal gas EoS!
template <int D>
class DensityIndependentSolver : public SolverBase<D> {
    static constexpr int dim = D;

private:
    class UDivv {
    private:
        ArrayView<const Float> e;
        ArrayView<const Vector> v;

    public:
        using Type = Float;

        void update(Storage& storage) {
            e = storage.getValue<Float>(QuantityIds::ENERGY_DENSITY);
            v = storage.getAll<Vector>(QuantityIds::POSITIONS)[1];
        }

        INLINE Tuple<Float, Float> operator()(const int i, const int j, const Vector& grad) const {
            const Float delta = dot(v[j] - v[i], grad);
            ASSERT(isReal(delta));
            return { e[j] * delta, e[i] * delta };
        }
    };
    Accumulator<UDivv> udivv;

public:
    DensityIndependentSolver(const GlobalSettings& settings)
        : SolverBase<D>(settings) {}

    virtual void integrate(Storage& storage, Statistics& UNUSED(stats)) override {
        const int size = storage.getParticleCnt();

        ArrayView<Vector> r, v, dv;
        ArrayView<Float> e, de, q, dq, rho, m, p, u, cs;
        PROFILE_SCOPE("DensityIndependentSolver::integrate (getters)");

        // fetch quantities from storage
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        tie(e, de) = storage.getAll<Float>(QuantityIds::ENERGY_PER_PARTICLE);
        tie(q, dq) = storage.getAll<Float>(QuantityIds::ENERGY_DENSITY);
        tie(rho, m, p, u, cs) = storage.getValues<Float>(QuantityIds::DENSITY,
            QuantityIds::MASSES,
            QuantityIds::PRESSURE,
            QuantityIds::ENERGY,
            QuantityIds::SOUND_SPEED);
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));

        udivv.update(storage);

        PROFILE_NEXT("DensityIndependentSolver::integrate (init)");
        // clamp smoothing length
        /// \todo generalize clamping, min / max values
        for (Float& h : componentAdapter(r, H)) {
            h = max(h, 1.e-12_f);
        }

        this->finder->build(r);
        SymH<dim> w(this->kernel);
        MaterialAccessor material(storage);

        PROFILE_NEXT("DensityIndependentSolver::compute (main cycle)")
        for (int i = 0; i < size; ++i) {
            this->finder->findNeighbours(i,
                r[i][H] * this->kernel.radius(),
                this->neighs,
                FinderFlags::FIND_ONLY_SMALLER_H | FinderFlags::PARALLELIZE);
            // iterate over neighbours
            for (const auto& neigh : this->neighs) {
                const int j = neigh.index;
                // actual smoothing length
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS && hbar <= r[i][H]);
                if (getSqrLength(r[i] - r[j]) > sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

                const Vector f = e[i] * e[j] * (1._f / q[i] + 1._f / q[j]) * grad;
                ASSERT(isReal(f));
                const Float gamma = material.getParam<Float>(BodySettingsIds::ADIABATIC_INDEX, i);
                ASSERT(gamma > 1._f);
                dv[i] -= (gamma - 1._f) / m[i] * f;
                dv[j] += (gamma - 1._f) / m[j] * f;
                ASSERT(isReal(dv[i]));
                ASSERT(isReal(dv[j]));

                udivv.accumulate(i, j, grad);
            }
        }

        EosAccessor eos(storage);
        for (int i = 0; i < size; ++i) {
            dq[i] = udivv[i];

            const Float gamma = material.getParam<Float>(BodySettingsIds::ADIABATIC_INDEX, i);
            de[i] = (gamma - 1._f) * e[i] / q[i] * udivv[i];
            /// \todo smoothing length
            v[i][H] = 0._f;
            dv[i][H] = 0._f;

            // compute 'external' physical quantities
            u[i] = e[i] / m[i];
            rho[i] = q[i] / u[i];

            tieToTuple(p[i], cs[i]) = eos.evaluate(i);
        }
        // Apply boundary conditions
        if (this->boundary) {
            this->boundary->apply(storage);
        }
    }

    virtual void initialize(Storage& storage, const BodySettings& settings) const override {
        ASSERT(settings.get<EosEnum>(BodySettingsIds::EOS) == EosEnum::IDEAL_GAS);
        // energy density = specific energy * density
        const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
        const Float u0 = settings.get<Float>(BodySettingsIds::ENERGY);
        const Float q0 = rho0 * u0;
        ASSERT(q0 > 0._f && "Cannot use DISPH with zero specific energy");
        const Range rhoRange = settings.get<Range>(BodySettingsIds::DENSITY_RANGE);
        const Range uRange = settings.get<Range>(BodySettingsIds::ENERGY_RANGE);
        const Range qRange(rhoRange.lower() * uRange.lower(), rhoRange.upper() * uRange.upper());
        const Float rhoMin = settings.get<Float>(BodySettingsIds::DENSITY_MIN);
        const Float uMin = settings.get<Float>(BodySettingsIds::ENERGY_MIN);
        const Float qMin = rhoMin * uMin;
        ASSERT(qRange.lower() > 0._f && "Cannot use DISPH with zero specific energy");
        storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY_DENSITY, q0, qRange);
        MaterialAccessor(storage).minimal(QuantityIds::ENERGY_DENSITY, 0) = qMin;

        // energy per particle
        Array<Float> e = storage.getValue<Float>(QuantityIds::MASSES).clone();
        for (Size i = 0; i < e.size(); ++i) {
            e[i] *= u0;
        }
        /// \todo range and min value for energy per particle
        storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY_PER_PARTICLE, std::move(e));

        // setup quantities used 'outside' of the solver
        storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::DENSITY, rho0);
        storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::ENERGY, u0);
        Float p0, cs0;
        tieToTuple(p0, cs0) = EosAccessor(storage).evaluate(0);
        storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::SOUND_SPEED, cs0);
        storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::PRESSURE, p0);
    }
};

NAMESPACE_SPH_END
