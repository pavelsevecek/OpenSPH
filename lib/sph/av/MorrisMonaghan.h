#pragma once

/// Time-dependent artificial viscosity by Morris & Monaghan (1997). Coefficient alpha and beta evolve in time
/// using computed derivatives for each particle separately.
/// Can be currently used only with standard scalar artificial viscosity.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/Accumulator.h"
#include "solvers/Module.h"
#include "storage/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


class MorrisMonaghanAV : public Module<Divv> {
private:
    ArrayView<Float> alpha, beta, dalpha, dbeta, cs, rho;
    ArrayView<Vector> r, v;
    const Float eps = 0.1_f;
    Divv divv;

public:
    MorrisMonaghanAV(const GlobalSettings&)
        : Module<Divv>(divv)
        , divv(QuantityKey::VELOCITY_DIVERGENCE) {}

    void initialize(Storage& storage, const BodySettings& settings) const {
        storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::AV_ALPHA,
            settings.get<Float>(BodySettingsIds::AV_ALPHA),
            settings.get<Range>(BodySettingsIds::AV_ALPHA_RANGE));
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::AV_BETA,
            settings.get<Float>(BodySettingsIds::AV_BETA),
            settings.get<Range>(BodySettingsIds::AV_BETA_RANGE));
        this->initializeModules(storage, settings);
    }

    void update(Storage& storage) {
        ArrayView<Vector> dv;
        tieToArray(r, v, dv) = storage.getAll<Vector>(QuantityKey::POSITIONS);
        tieToArray(alpha, dalpha) = storage.getAll<Float>(QuantityKey::AV_ALPHA);
        tieToArray(beta, dbeta) = storage.getAll<Float>(QuantityKey::AV_BETA);
        cs = storage.getValue<Float>(QuantityKey::SOUND_SPEED);
        rho = storage.getValue<Float>(QuantityKey::DENSITY);
        // always keep beta = 2*alpha
        for (int i = 0; i < alpha.size(); ++i) {
            beta[i] = 2._f * alpha[i];
        }
        this->updateModules(storage);
    }

    INLINE void accumulate(const int i, const int j, const Vector& grad) {
        this->accumulateModules(i, j, grad);
    }

    INLINE void integrate(Storage& storage) {
        Range bounds = storage.getValue<Float>(QuantityKey::AV_ALPHA).getBounds();
        for (int i = 0; i < storage.getParticleCnt(); ++i) {
            const Float tau = r[i][H] / (eps * cs[i]);
            const Float decayTerm = -(alpha[i] - bounds.getLower()) / tau;
            const Float sourceTerm = Math::max(-(bounds.getUpper() - alpha[i]) * divv[i], 0._f);
            dalpha[i] = decayTerm + sourceTerm;
        }
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
};


NAMESPACE_SPH_END
