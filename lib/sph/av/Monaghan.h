#pragma once

/// Standard artificial viscosity by Monaghan (1989), using a velocity divergence in linear and quadratic term
/// as a measure of local (scalar) viscosity. Parameters alpha_AV and beta_AV are constant (in time) and equal
/// for all particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class MonaghanAV : public Object {
private:
    Float alpha, beta;
    const Float eps = 1.e-2_f;

public:
    MonaghanAV(const Settings<GlobalSettingsIds>& settings) {
        alpha = settings.get<Float>(GlobalSettingsIds::AV_ALPHA);
        beta  = settings.get<Float>(GlobalSettingsIds::AV_BETA);
    }

    INLINE Float
    operator()(const Vector& dv, const Vector& dr, const Float csbar, const Float rhobar, const Float hbar) {
        const Float dvdr = dot(dv, dr);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float mu = hbar * dvdr / (getSqrLength(dr) + eps * Math::sqr(hbar));
        return 1._f / rhobar * (-alpha * csbar * mu + beta * Math::sqr(mu));
    }
};


NAMESPACE_SPH_END
