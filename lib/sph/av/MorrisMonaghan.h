#pragma once

/// Time-dependent artificial viscosity by Morris & Monaghan (1997). Coefficient alpha and beta evolve in time
/// using computed derivatives for each particle separately.
/// Can be currently used only with standard scalar artificial viscosity.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// must have 2 parts, accumulate: executed for every 2 particles (must not be virtual), and derive, compute
/// derivatvies (can be virtual)

class MorrisMonaghanAV : public Object {
private:
    ArrayView<Float> alpha;
    ArrayView<Float> beta;
    ArrayView<Float> dalpha;
    ArrayView<Float> dbeta;
    const Float eps = 0.1_f;

public:
    MorrisMonaghanAV(const Settings<GlobalSettingsIds>& settings) {}

    INLINE Float operator()(const int i, const int j) {
        const Float dvdr = dot(dv, dr);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float alphabar = 0.5_f * (alpha[i] + alpha[j]);
        const Float betabar  = 0.5_f * (beta[i] + beta[j]);
        const Float mu       = hbar * dvdr / (getSqrLength(dr) + eps * Math::sqr(hbar));
        return 1._f / rhobar * (-alphabar * csbar * mu + betabar * Math::sqr(mu));
    }

    /// virtual, tady to jde v cajku
    INLINE void derive() {
        for (int i = 0; i < size; ++i) {
            const Float tau    = r[i][H] / (eps * cs[i]);
            const Range bounds = alpha.getBounds();
            dalpha[i]          = -(alpha[i] - bounds.getLower()) / tau +
                        Math::max(-(bounds.getUpper() - alpha[i]) * divv[i], 0._f);
        }
    }
};


NAMESPACE_SPH_END
