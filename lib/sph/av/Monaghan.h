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
    operator()(const Vector& dv, const Vector& dr, const Float cs, const Float rhobar, const Float hbar) {
        const Float mu = hbar * dot(dv, dr) / (getSqrLength(dr) + eps * Math::sqr(hbar));
        return 1._f / rhobar * (-alpha * cs * mu + beta * Math::sqr(mu));
    }
};

NAMESPACE_SPH_END
