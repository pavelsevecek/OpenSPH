#pragma once

#include "objects/wrappers/Flags.h"
#include "solvers/Accumulator.h"
#include "storage/QuantityMap.h"
#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN

/// Object computing acceleration of particles and increase of internal energy due to divergence of the stress
/// tensor. When no stress tensor is used in the model, only pressure gradient is computed.
template <typename Yielding, typename Damage, typename AV>
class StressForce : public Object {
private:
    Accumulator<RhoDivv> rhoDivv;
    Accumulator<RhoGradv> rhoGradv;
    ArrayView<Float> p, rho, du, u, m;
    ArrayView<Vector> v, dv;
    ArrayView<TracelessTensor> s, ds;

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
        : av(settings) {
        flags.setIf(Options::USE_GRAD_P, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_GRAD_P));
        flags.setIf(Options::USE_DIV_S, settings.get<bool>(GlobalSettingsIds::MODEL_FORCE_DIV_S));
        // cannot use stress tensor without pressure
        ASSERT(!(flags.has(Options::USE_DIV_S) && !flags.has(Options::USE_GRAD_P)));
    }

    StressForce(const StressForce& other) = delete;

    StressForce(StressForce&& other) = default;

    void update(Storage& storage) {
        tieToArray(rho, u, m) = storage.getValues<Float>(QuantityKey::RHO, QuantityKey::U, QuantityKey::M);
        ArrayView<Vector> r;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        if (flags.has(Options::USE_GRAD_P)) {
            p = storage.getValue<Float>(QuantityKey::P);
        }
        if (flags.has(Options::USE_DIV_S)) {
            tieToArray(s, ds) = storage.getAll<TracelessTensor>(QuantityKey::S);
        }
        rhoDivv.update(storage);
        rhoGradv.update(storage);
        damage.update(storage);
        yielding.update(storage);
    }

    INLINE void accumulate(const int i, const int j, const Vector& grad) {
        Vector f(0._f);
        const Float rhoInvSqri = 1._f / Math::sqr(rho[i]);
        const Float rhoInvSqrj = 1._f / Math::sqr(rho[j]);
        if (flags.has(Options::USE_GRAD_P)) {
            /// \todo measure if these branches have any effect on performance
            const auto avij = av(i, j);
            f -= (reduce(p[i], i) * rhoInvSqri + reduce(p[j], i) * rhoInvSqrj + avij) * grad;
            rhoDivv.accumulate(i, j, grad);
            // account for shock heating
            const Float heating = 0.5_f * avij * dot(v[i] - v[j], grad);
            du[i] += m[j] * heating;
            du[j] += m[i] * heating;
        }
        if (flags.has(Options::USE_DIV_S)) {
            f += (reduce(s[i], i) * rhoInvSqri + reduce(s[j], i) * rhoInvSqrj) * grad;
            rhoGradv.accumulate(i, j, grad);
        }
        dv[i] += m[j] * f;
        dv[j] -= m[i] * f;
        // internal energy is computed at the end using accumulated values
    }

    void evaluate(Storage& storage) {
        for (int i = 0; i < du.size(); ++i) {
            /// \todo check correct sign
            const Float rhoInvSqr = 1._f / Math::sqr(rho[i]);
            if (flags.has(Options::USE_GRAD_P)) {
                du[i] -= reduce(p[i], i) * rhoInvSqr * rhoDivv[i];
            }
            if (flags.has(Options::USE_DIV_S)) {
                du[i] += rhoInvSqr * ddot(reduce(s[i], i), rhoGradv[i]);

                // compute derivatives of the stress tensor
                /// \todo rotation rate tensor?
                const Float mu = storage.getMaterial(i).shearModulus;
                /// \todo how to enforce that this expression is traceless tensor?
                ds[i] += 2._f * mu * (rhoGradv[i] - Tensor::identity() * rhoGradv[i].trace() / 3._f);
                ASSERT(Math::isReal(ds[i]));

                // compute derivatives of damage
                damage.integrate(storage);
            }
            ASSERT(Math::isReal(du[i]));
        }
    }

    QuantityMap getQuantityMap() const {
        QuantityMap map;
        map[QuantityKey::U] = { ValueEnum::SCALAR, OrderEnum::FIRST_ORDER };
        if (flags.has(Options::USE_GRAD_P)) {
            map[QuantityKey::P] = { ValueEnum::SCALAR, OrderEnum::ZERO_ORDER };
        }
        if (flags.has(Options::USE_DIV_S)) {
            map[QuantityKey::S] = { ValueEnum::TRACELESS_TENSOR, OrderEnum::FIRST_ORDER };
        }
        return map;
    }

private:
    /// \todo possibly precompute damage / yielding
    INLINE auto reduce(const Float pi, const int idx) const { return damage(pi, idx); }

    INLINE auto reduce(const TracelessTensor& si, const int idx) const {
        // first apply damage
        auto si_dmg = damage(si, idx);
        // then apply yielding using reduced stress
        return yielding(si_dmg, idx);
    }
};

NAMESPACE_SPH_END
