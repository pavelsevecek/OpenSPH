#pragma once

/// \file MorrisMonaghan.h
/// \brief Time-dependent artificial viscosity by Morris & Monaghan (1997).
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/AbstractMaterial.h"
#include "quantities/Storage.h"
#include "sph/equations/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Time-dependent artificial viscosity with non-homogeneous oefficients alpha and beta
///
/// Coefficient alpha and beta evolve in time using computed derivatives for each particle separately.
/// Although the same mechanism could be used with any artificial viscosity, the current implementation is only an extension of the standard scalar artificial viscosity.
class MorrisMonaghanAV : public Abstract::EquationTerm {
private:
    class Derivative : public Abstract::Derivative {
    private:
        ArrayView<const Float> alpha, beta, dalpha, dbeta, cs, rho;
        ArrayView<const Vector> r, v;
        ArrayView<Vector> dv;
        ArrayView<Float> du;
        const Float eps = 0.1_f;

    public:
        virtual void initialize(const Storage& input, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = storage.getAll<Vector>(QuantityId::POSITIONS);
            tie(alpha, dalpha) = storage.getAll<Float>(QuantityId::AV_ALPHA);
            tie(beta, dbeta) = storage.getAll<Float>(QuantityId::AV_BETA);
            cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
            rho = storage.getValue<Float>(QuantityId::DENSITY);
            
            dv = results.getValue<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            du = results.getValue<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        virtual void compute(const Size i,
            ArrayView<const NeighbourRecord> neighs,
            ArrayView<const Vector> grads) override {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k].index;
                const Vector dr = r[i] - r[j];
                const Float dvdr = dot(v[i] - v[j], dr);
                if (dvdr >= 0._f) {
                    continue;
                }
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                const Float csbar = 0.5_f * (cs[i] + cs[j]);
                const Float rhobar = 0.5_f * (rho[i] + rho[j]);
                const Float alphabar = 0.5_f * (alpha[i] + alpha[j]);
                const Float betabar = 0.5_f * (beta[i] + beta[j]);
                const Float mu = hbar * dvdr / (getSqrLength(dr) + eps * sqr(hbar));
                const Float Pi = this->operator ()(i, j);

                dv[i] += m[j] * Pi * grads[i];
                dv[j] -= m[i] * Pi * grads[i];

                const Float heating = Pi * dot(v[i] - v[j], grads[k]);
                dv[i] += m[j] * heating;
                dv[j] += m[i] * heating;
            }
        }
		
		INLINE Float operator()(const Size i, const Size j) const {
            const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
            if (dvdr >= 0._f) {
                return 0._f;
            }
           const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                const Float csbar = 0.5_f * (cs[i] + cs[j]);
                const Float rhobar = 0.5_f * (rho[i] + rho[j]);
                const Float alphabar = 0.5_f * (alpha[i] + alpha[j]);
                const Float betabar = 0.5_f * (beta[i] + beta[j]);
                const Float mu = hbar * dvdr / (getSqrLength(dr) + eps * sqr(hbar));
                return 1._f / rhobar * (-alphabar * csbar * mu + betabar * sqr(mu));

        } 
    };

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.addDerivative<Term>(settings);
		derivatives.addDerivative<VelocityDivergence>(settings);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        tie(alpha, dalpha) = storage.getAll<Float>(QuantityId::AV_ALPHA);
        tie(beta, dbeta) = storage.getAll<Float>(QuantityId::AV_BETA);
        cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
        rho = storage.getValue<Float>(QuantityId::DENSITY);
        // always keep beta = 2*alpha
        for (Size i = 0; i < alpha.size(); ++i) {
            beta[i] = 2._f * alpha[i];
        }        
    }

    virtual void finalize(Storage& (storage) override {
		ArrayView<Float> alpha, dalpha;
		tie(alpha, dalpha) = storage.getAll<Float>(QuantityId::AV_ALPHA);
		ArrayView<Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            const Range bounds = material.getParam<Range>(BodySettingsId::AV_ALPHA_RANGE, i);
            const Float tau = r[i][H] / (eps * cs[i]);
            const Float decayTerm = -(alpha[i] - Float(bounds.lower())) / tau;
            const Float sourceTerm = max(-(Float(bounds.upper()) - alpha[i]) * divv[i], 0._f);
            dalpha[i] = decayTerm + sourceTerm;
        }
    }
	
	virtual void create(Storage& storage, Abstract::Material& material) const override {}
        storage.insert<Float, OrderEnum::FIRST>(QuantityId::AV_ALPHA, settings.get<Float>(BodySettingsId::AV_ALPHA));
        storage.insert<Float, OrderEnum::ZERO>(QuantityId::AV_BETA, settings.get<Float>(BodySettingsId::AV_BETA));
            
		material.range(QuantityId::AV_ALPHA, settings.get<Range>(BodySettingsId::AV_ALPHA_RANGE));
    }
};

NAMESPACE_SPH_END
