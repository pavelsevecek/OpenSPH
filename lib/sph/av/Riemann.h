#pragma once

/// Artificial viscosity based on Riemann solver.
/// see Monaghan (1997), SPH and Riemann Solvers, J. Comput. Phys. 136, 298
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "quantities/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class RiemannAV  {
private:
    Float alpha;
    ArrayView<Vector> r, v;
    ArrayView<Float> cs, rho;

public:
    RiemannAV(const GlobalSettings& settings) {
        alpha = settings.get<Float>(GlobalSettingsIds::SPH_AV_ALPHA);
    }

    void update(Storage& storage) {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
        rho = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
    }

    INLINE Float operator()(const int i, const int j) {
        const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float w = dvdr / getLength(r[i] - r[j]);
        const Float vsig = cs[i] + cs[j] - 3._f * w;
        const Float rhobar = 0.5_f * (rho[i] + rho[j]);
        return -0.5_f * alpha * vsig * w / rhobar;
    }
};


NAMESPACE_SPH_END
