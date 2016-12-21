#pragma once

/// Artificial viscosity based on Riemann solver.
/// see Monaghan (1997), SPH and Riemann Solvers, J. Comput. Phys. 136, 298
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "storage/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class RiemannAV : public Object {
private:
    Float alpha;
    ArrayView<Vector> r, v;
    ArrayView<Float> cs, rho;

public:
    RiemannAV(const GlobalSettings&) {
        alpha = 1.5_f; /// \todo from body settinds
        // settings.get<Float>(::AV_ALPHA);
    }

    void update(Storage& storage) {
        ArrayView<Vector> dv;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        cs = storage.getValue<Float>(QuantityKey::CS);
        rho = storage.getValue<Float>(QuantityKey::CS);
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

    INLINE void accumulate(const int UNUSED(i), const int UNUSED(j), const Vector& UNUSED(grad)) {}
};


NAMESPACE_SPH_END
