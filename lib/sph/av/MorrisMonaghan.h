#pragma once

/// Time-dependent artificial viscosity by Morris & Monaghan (1997). Coefficient alpha and beta evolve in time
/// using computed derivatives for each particle separately.
/// Can be currently used only with standard scalar artificial viscosity.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "quantities/AbstractMaterial.h"
#include "quantities/Storage.h"
#include "solvers/Derivative.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


class MorrisMonaghanAV : public Abstract::EquationTerm {
private:
    ArrayView<Float> alpha, beta, dalpha, dbeta, cs, rho;
    ArrayView<Vector> r, v;
    const Float eps = 0.1_f;

public:
    MorrisMonaghanAV(const GlobalSettings&) {
        class Term : public Abstract::Derivative {

            virtual void initialize(Storage& storage) override {
                NOT_IMPLEMENTED;
            }

            virtual void sum() override {
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
                const Float mu = hbar * dvdr / (getSqrLength(dr) + eps * sqr(hbar));
                const Float Pi = 1._f / rhobar * (-alphabar * csbar * mu + betabar * sqr(mu));

                dv[i] += m[j] * Pi * grads[i];
                dv[j] -= m[i] * Pi * grads[i];

                const Float heating = Pi * dot(v[i] - v[j], grads[i]);
                dv[i] += m[j] * heating;
                dv[j] += m[i] * heating;
            }
        };

        solver.addDerivative(std::unique_ptr<Term>());
        solver.addDerivative(std::unique_ptr<Divv>());
    }

    void initialize(Storage& storage, const BodySettings& settings) const {
        storage.insert<Float, OrderEnum::FIRST>(QuantityIds::AV_ALPHA,
            settings.get<Float>(BodySettingsIds::AV_ALPHA),
            settings.get<Range>(BodySettingsIds::AV_ALPHA_RANGE));
        storage.insert<Float, OrderEnum::ZERO>(QuantityIds::AV_BETA,
            settings.get<Float>(BodySettingsIds::AV_BETA),
            settings.get<Range>(BodySettingsIds::AV_BETA_RANGE));
        this->initializeModules(storage, settings);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        tie(alpha, dalpha) = storage.getAll<Float>(QuantityIds::AV_ALPHA);
        tie(beta, dbeta) = storage.getAll<Float>(QuantityIds::AV_BETA);
        cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
        rho = storage.getValue<Float>(QuantityIds::DENSITY);
        // always keep beta = 2*alpha
        for (Size i = 0; i < alpha.size(); ++i) {
            beta[i] = 2._f * alpha[i];
        }
        this->updateModules(storage);
    }

    INLINE void accumulate(const Size i, const Size j, const Vector& grad) {
        this->accumulateModules(i, j, grad);
    }

    INLINE virtual void finalize(Storage& storage) override {
        MaterialAccessor material(storage);
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            const Range bounds = material.getParam<Range>(BodySettingsIds::AV_ALPHA_RANGE, i);
            const Float tau = r[i][H] / (eps * cs[i]);
            const Float decayTerm = -(alpha[i] - Float(bounds.lower())) / tau;
            const Float sourceTerm = max(-(Float(bounds.upper()) - alpha[i]) * divv[i], 0._f);
            dalpha[i] = decayTerm + sourceTerm;
        }
    }
};


NAMESPACE_SPH_END
