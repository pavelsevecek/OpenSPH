#pragma once

#include "objects/wrappers/Flags.h"
#include "quantities/Material.h"
#include "quantities/Storage.h"
#include "solvers/Accumulator.h"
#include "solvers/Module.h"

NAMESPACE_SPH_BEGIN

/// Object computing acceleration of particles and increase of internal energy due to divergence of the stress
/// tensor. When no stress tensor is used in the model, only pressure gradient is computed.
template <typename Yielding, typename Damage, typename AV>
class StressForce : public Module<Yielding, Damage, AV, RhoDivv, RhoGradv> {
private:
    RhoDivv rhoDivv;
    RhoGradv rhoGradv;
    ArrayView<Float> p, rho, du, u, m, cs;
    ArrayView<Vector> v, dv;
    ArrayView<TracelessTensor> s, ds;
    ArrayView<Size> bodyIdxs;

    enum class Options {
        USE_GRAD_P = 1 << 0,
        USE_DIV_S = 1 << 1,
    };
    Flags<Options> flags;

    Damage damage;
    Yielding yielding;
    AV av;

public:
    StressForce(const GlobalSettings& settings)
        : Module<Yielding, Damage, AV, RhoDivv, RhoGradv>(yielding, damage, av, rhoDivv, rhoGradv)
        , rhoGradv(QuantityIds::RHO_GRAD_V)
        , damage(settings)
        , av(settings) {
        flags.setIf(Options::USE_GRAD_P, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_GRAD_P));
        flags.setIf(Options::USE_DIV_S, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_DIV_S));
        // cannot use stress tensor without pressure
        ASSERT(!(flags.has(Options::USE_DIV_S) && !flags.has(Options::USE_GRAD_P)));
    }

    StressForce(const StressForce& other) = delete;

    StressForce(StressForce&& other) = default;

    void update(Storage& storage) {
        tie(rho, m) = storage.getValues<Float>(QuantityIds::DENSITY, QuantityIds::MASSES);
        tie(u, du) = storage.getAll<Float>(QuantityIds::ENERGY);
        ArrayView<Vector> r;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        if (flags.has(Options::USE_GRAD_P)) {
            p = storage.getValue<Float>(QuantityIds::PRESSURE);
            cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
            // compute new values of pressure and sound speed
            EosAccessor eos(storage);
            for (Size i = 0; i < r.size(); ++i) {
                tieToTuple(p[i], cs[i]) = eos.evaluate(i);
            }
        }
        if (flags.has(Options::USE_DIV_S)) {
            tie(s, ds) = storage.getAll<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        }
        bodyIdxs = storage.getValue<Size>(QuantityIds::FLAG);
        this->updateModules(storage);
    }

    INLINE void accumulate(const Size i, const Size j, const Vector& grad) {
        Vector f(0._f);
        // const Float rhoInvSqri = 1._f / sqr(rho[i]);
        // const Float rhoInvSqrj = 1._f / sqr(rho[j]);
        if (flags.has(Options::USE_GRAD_P)) {
            /// \todo measure if these branches have any effect on performance
            const auto avij = av(i, j);
            // f -= (reduce(p[i], i) * rhoInvSqri + reduce(p[j], i) * rhoInvSqrj + avij) * grad;
            f -= ((reduce(p[i], i) + reduce(p[j], j)) / (rho[i] * rho[j]) + avij) * grad;
            // account for shock heating
            const Float heating = 0.5_f * avij * dot(v[i] - v[j], grad);
            du[i] += m[j] * heating;
            du[j] += m[i] * heating;
        }
        if (flags.has(Options::USE_DIV_S) && bodyIdxs[i] == bodyIdxs[j]) {
            // apply stress only if particles belong to the same body
            f += (reduce(s[i], i) + reduce(s[j], i)) / (rho[i] * rho[j]) * grad;
        }
        dv[i] += m[j] * f;
        dv[j] -= m[i] * f;
        // internal energy is computed at the end using accumulated values
        this->accumulateModules(i, j, grad);
    }

    void integrate(Storage& storage) {
        MaterialAccessor material(storage);
        for (Size i = 0; i < du.size(); ++i) {
            /// \todo check correct sign
            // const Float rhoInvSqr = 1._f / sqr(rho[i]);
            if (flags.has(Options::USE_GRAD_P)) {
                du[i] -= reduce(p[i], i) / rho[i] * rhoDivv[i];
            }
            if (flags.has(Options::USE_DIV_S)) {
                du[i] += 1._f / rho[i] * ddot(reduce(s[i], i), rhoGradv[i]);

                // compute derivatives of the stress tensor
                /// \todo rotation rate tensor?
                const Float mu = material.getParam<Float>(BodySettingsIds::SHEAR_MODULUS, i);
                /// \todo how to enforce that this expression is traceless tensor?
                ds[i] += TracelessTensor(
                    2._f * mu * (rhoGradv[i] - Tensor::identity() * rhoGradv[i].trace() / 3._f));
                ASSERT(isReal(ds[i]));
            }
            ASSERT(isReal(du[i]));
        }
        this->integrateModules(storage);
    }

    void initialize(Storage& storage, const BodySettings& settings) const {
        storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::ENERGY,
            settings.get<Float>(BodySettingsIds::ENERGY),
            settings.get<Range>(BodySettingsIds::ENERGY_RANGE));
        MaterialAccessor(storage).minimal(QuantityIds::ENERGY, 0) =
            settings.get<Float>(BodySettingsIds::ENERGY_MIN);
        if (flags.has(Options::USE_GRAD_P)) {
            // Compute pressure using equation of state
            std::unique_ptr<Abstract::Eos> eos = Factory::getEos(settings);
            const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
            const Float u0 = settings.get<Float>(BodySettingsIds::ENERGY);
            const Size n = storage.getParticleCnt();
            Array<Float> p(n), cs(n);
            for (Size i = 0; i < n; ++i) {
                tieToTuple(p[i], cs[i]) = eos->evaluate(rho0, u0);
            }
            storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::PRESSURE, std::move(p));
            storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::SOUND_SPEED, std::move(cs));
        }
        if (flags.has(Options::USE_DIV_S)) {
            storage.insert<TracelessTensor, OrderEnum::FIRST_ORDER>(QuantityIds::DEVIATORIC_STRESS,
                settings.get<TracelessTensor>(BodySettingsIds::STRESS_TENSOR),
                Range::unbounded());
            MaterialAccessor(storage).minimal(QuantityIds::DEVIATORIC_STRESS, 0) =
                settings.get<Float>(BodySettingsIds::STRESS_TENSOR_MIN);
            storage.insert<Tensor, OrderEnum::ZERO_ORDER>(QuantityIds::RHO_GRAD_V, Tensor::null());
            MaterialAccessor material(storage);
            material.setParams(BodySettingsIds::SHEAR_MODULUS, settings);
        }
        this->initializeModules(storage, settings);
    }

private:
    /// \todo possibly precompute damage / yielding
    INLINE auto reduce(const Float pi, const Size idx) const {
        return damage.reduce(pi, idx);
    }

    INLINE auto reduce(const TracelessTensor& si, const Size idx) const {
        return damage.reduce(si, idx);
    }
};

NAMESPACE_SPH_END
