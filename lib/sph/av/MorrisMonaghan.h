#pragma once

/// Time-dependent artificial viscosity by Morris & Monaghan (1997). Coefficient alpha and beta evolve in time
/// using computed derivatives for each particle separately.
/// Can be currently used only with standard scalar artificial viscosity.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/Accumulator.h"
#include "storage/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// must have 2 parts, accumulate: executed for every 2 particles (must not be virtual), and derive, compute
/// derivatvies (can be virtual)

class MorrisMonaghanAV : public Object {
private:
    ArrayView<Float> alpha, beta, dalpha, dbeta, cs, rho;
    ArrayView<Vector> r, v;
    const Float eps = 0.1_f;
    Range bounds; /// \todo get bounds from body settings

    Accumulator<Divv> divv;

public:
    MorrisMonaghanAV(const GlobalSettings&) {}

    void update(Storage& storage) {
        ArrayView<Vector> dv;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::R);
        tieToArray(alpha, dalpha) = storage.getAll<Float>(QuantityKey::ALPHA);
        tieToArray(beta, dbeta) = storage.getAll<Float>(QuantityKey::BETA);
        cs = storage.getValue<Float>(QuantityKey::CS);
        rho = storage.getValue<Float>(QuantityKey::RHO);
        divv.update(storage);
    }

    INLINE Float operator()(const int i, const int j) {
        const Vector dr = r[i] - r[j];
        const Float dvdr = dot(v[i] - v[j], dr);
        if (dvdr >= 0._f) {
            return 0._f;
        }
        const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
        const Float csbar = 0.5_f * (cs[i] + cs[j]);
        const Float rhobar = 0.5_f * (rho[i] + rho[j]);
        const Float alphabar = 0.5_f * (alpha[i] + alpha[j]);
        const Float betabar = 0.5_f * (beta[i] + beta[j]);
        const Float mu = hbar * dvdr / (getSqrLength(dr) + eps * Math::sqr(hbar));
        return 1._f / rhobar * (-alphabar * csbar * mu + betabar * Math::sqr(mu));
    }

    INLINE void evaluate(Storage& storage) {
        for (int i = 0; i < storage.getParticleCnt(); ++i) {
            const Float tau = r[i][H] / (eps * cs[i]);
            const Float decayTerm = -(alpha[i] - bounds.getLower()) / tau;
            const Float sourceTerm = Math::max(-(bounds.getUpper() - alpha[i]) * divv[i], 0._f);
            dalpha[i] = decayTerm + sourceTerm;
        }
    }
};


NAMESPACE_SPH_END
