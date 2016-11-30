#pragma once

/// Artificial viscosity based on Riemann solver.
/// see Monaghan (1997), SPH and Riemann Solvers, J. Comput. Phys. 136, 298
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class RiemannAV : public Object {
private:
    Float alpha;
    Array<Vector>& r;
    Array<Vector>& v;
    Array<Float>& cs;
    Array<Float>& rho;

public:
    RiemannAV(Storage& storage, const Settings<GlobalSettingsIds>& settings) 
        : r(storage.get<QuantityKey::R, Vector, TemporalEnum::SECOND_ORDER>())
        , v(storage.dt<QuantityKey::R, Vector>())
        , cs(storage.get<QuantityKey::CS, Float>()) {
        alpha = settings.get<Float>(GlobalSettingsIds::AV_ALPHA);
    }

    INLINE Float operator()(const int i, const int j) {
        const Float dvdr = dot(v[i]-v[j], r[i]-r[j]);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float w = dvdr / getLength(r[i] - r[j]);
        const Float vsig = cs[i] + cs[j] - 3._f * w;
        const Float rhobar = 0.5_f * (rho[i] + rho[j])
        return -0.5_f * alpha * vsig * w / rhobar;
    }
};


NAMESPACE_SPH_END
