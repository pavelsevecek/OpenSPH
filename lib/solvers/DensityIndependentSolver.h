#pragma once

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
            e = storage.getValue<Float>(QuantityKey::ENERGY_DENSITY);
            v = storage.getAll<Vector>(QuantityKey::POSITIONS)[1];
        }

        INLINE Tuple<Float, Float> operator()(const int i, const int j, const Vector& grad) const {
            const Float delta = dot(v[j] - v[i], grad);
            ASSERT(Math::isReal(delta));
            return { e[j] * delta, e[i] * delta };
        }
    };
    Accumulator<UDivv> udivv;

public:
    DensityIndependentSolver(const GlobalSettings& settings)
        : SolverBase<D>(settings) {}

    virtual void integrate(Storage& storage) override {
        const int size = storage.getParticleCnt();

        ArrayView<Vector> r, v, dv;
        ArrayView<Float> e, de, q, dq, rho, m, p, u, cs;
        PROFILE_SCOPE("DensityIndependentSolver::integrate (getters)");

        // fetch quantities from storage
        tie(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
        tie(e, de) = storage.getAll<Float>(QuantityKey::ENERGY_PER_PARTICLE);
        tie(q, dq) = storage.getAll<Float>(QuantityKey::ENERGY_DENSITY);
        tie(rho, m, p, u, cs) = storage.getValues<Float>(QuantityKey::DENSITY,
            QuantityKey::MASSES,
            QuantityKey::PRESSURE,
            QuantityKey::ENERGY,
            QuantityKey::SOUND_SPEED);
        ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));

        udivv.update(storage);

        PROFILE_NEXT("DensityIndependentSolver::integrate (init)");
        // clamp smoothing length
        /// \todo generalize clamping, min / max values
        for (Float& h : componentAdapter(r, H)) {
            h = Math::max(h, 1.e-12_f);
        }

        this->finder->build(r);
        SymH<dim> w(this->kernel);

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
                if (getSqrLength(r[i] - r[j]) > Math::sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                // compute gradient of kernel W_ij
                const Vector grad = w.grad(r[i], r[j]);
                ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

                const Vector f = e[i] * e[j] * (1._f / q[i] + 1._f / q[j]) * grad;
                ASSERT(Math::isReal(f));
                const Float gamma = storage.getMaterial(i).adiabaticIndex;
                ASSERT(gamma > 1._f);
                dv[i] -= (gamma - 1._f) / m[i] * f;
                dv[j] += (gamma - 1._f) / m[j] * f;
                ASSERT(Math::isReal(dv[i]));
                ASSERT(Math::isReal(dv[j]));

                udivv.accumulate(i, j, grad);
            }
        }

        for (int i = 0; i < size; ++i) {
            dq[i] = udivv[i];

            const Float gamma = storage.getMaterial(i).adiabaticIndex;
            de[i] = (gamma - 1._f) * e[i] / q[i] * udivv[i];
            /// \todo smoothing length
            v[i][H] = 0._f;
            dv[i][H] = 0._f;

            // compute 'external' physical quantities
            u[i] = e[i] / m[i];
            rho[i] = q[i] / u[i];

            tieToTuple(p[i], cs[i]) = storage.getMaterial(i).eos->getPressure(rho[i], u[i]);
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
        ASSERT(qRange.lower() > 0._f && "Cannot use DISPH with zero specific energy");
        storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::ENERGY_DENSITY, q0, qRange);

        // energy per particle
        Array<Float> e = storage.getValue<Float>(QuantityKey::MASSES).clone();
        for (Size i = 0; i < e.size(); ++i) {
            e[i] *= u0;
        }
        storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::ENERGY_PER_PARTICLE, std::move(e));

        // setup quantities used 'outside' of the solver
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::DENSITY, rho0);
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::ENERGY, u0);
        Float p0, cs0;
        tieToTuple(p0, cs0) = storage.getMaterial(0).eos->getPressure(rho0, u0);
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::SOUND_SPEED, cs0);
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::PRESSURE, p0);
    }
};

NAMESPACE_SPH_END
