#pragma once

/// Standard artificial viscosity by Monaghan (1989), using a velocity divergence in linear and quadratic term
/// as a measure of local (scalar) dissipation. Parameters alpha_AV and beta_AV are constant (in time) and
/// equal for all particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "system/Settings.h"
#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN

class StandardAV : public Object {
private:
    Float alpha, beta;
    ArrayView<const Vector> r, v;
    ArrayView<const Float> rho, cs;
    const Float eps = 1.e-2_f;

public:
    StandardAV(const std::shared_ptr<Storage>& storage, const Settings<GlobalSettingsIds>& settings) {
        refs(r, v) = storage->get<QuantityKey::R, Vector, OrderEnum::FIRST_ORDER>();
        refs(rho) = storage->get<QuantityKey::RHO, Float, OrderEnum::ZERO_ORDER>();
        refs(cs) = storage->get<QuantityKey::CS, Float, OrderEnum::ZERO_ORDER>();

        alpha = settings.get<Float>(GlobalSettingsIds::SPH_AV_ALPHA);
        beta  = settings.get<Float>(GlobalSettingsIds::SPH_AV_BETA);
    }

    INLINE Float operator()(const int i, const int j) {
        const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float hbar   = 0.5_f * (r[i][H] + r[j][H]);
        const Float rhobar = 0.5_f * (rho[i] + rho[j]);
        const Float csbar  = 0.5_f * (cs[i] + cs[j]);
        const Float mu     = hbar * dvdr / (getSqrLength(r[i] - r[j]) + eps * Math::sqr(hbar));
        return 1._f / rhobar * (-alpha * csbar * mu + beta * Math::sqr(mu));
    }
};


NAMESPACE_SPH_END
