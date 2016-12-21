#pragma once

/// Standard artificial viscosity by Monaghan (1989), using a velocity divergence in linear and quadratic term
/// as a measure of local (scalar) dissipation. Parameters alpha_AV and beta_AV are constant (in time) and
/// equal for all particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "storage/Storage.h"
#include "system/Factory.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class StandardAV : public Object {
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
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        tieToArray(rho, cs) = storage.getValues<Float>(QuantityKey::RHO, QuantityKey::CS);
        u = storage.getValue<Float>(QuantityKey::U);
        for (int i = 0; i < cs.size(); ++i) {
            /// \todo update sound speed together with pressure
            tieToTuple(IGNORE, cs[i]) = storage.getMaterial(i).eos->getPressure(rho[i], u[i]);
        }
    }

    INLINE void accumulate(const int UNUSED(i), const int UNUSED(j), const Vector& UNUSED(grad)) {}

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
