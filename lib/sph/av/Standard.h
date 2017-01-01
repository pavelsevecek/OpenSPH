#pragma once

/// Standard artificial viscosity by Monaghan (1989), using a velocity divergence in linear and quadratic term
/// as a measure of local (scalar) dissipation. Parameters alpha_AV and beta_AV are constant (in time) and
/// equal for all particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "physics/Eos.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class StandardAV  {
private:
    Float alpha, beta;
    ArrayView<const Vector> r, v;
    ArrayView<Float> rho, cs;
    const Float eps = 1.e-2_f;

public:
    StandardAV(const GlobalSettings& settings) {
        alpha = settings.get<Float>(GlobalSettingsIds::SPH_AV_ALPHA);
        beta = settings.get<Float>(GlobalSettingsIds::SPH_AV_BETA);
    }

    void update(Storage& storage) {
        ArrayView<const Vector> dv;
        ArrayView<const Float> u;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
        // sound speed must be computed by the solver using AV
        tie(rho, cs) = storage.getValues<Float>(QuantityKey::DENSITY, QuantityKey::SOUND_SPEED);
    }

    INLINE Float operator()(const int i, const int j) {
        const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
        const Float rhobar = 0.5_f * (rho[i] + rho[j]);
        const Float csbar = 0.5_f * (cs[i] + cs[j]);
        const Float mu = hbar * dvdr / (getSqrLength(r[i] - r[j]) + eps * Math::sqr(hbar));
        return 1._f / rhobar * (-alpha * csbar * mu + beta * Math::sqr(mu));
    }
};


NAMESPACE_SPH_END
