#pragma once

/// \file MorrisMonaghan.h
/// \brief Time-dependent artificial viscosity by Morris & Monaghan (1997).
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "sph/equations/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Time-dependent artificial viscosity with non-homogeneous oefficients alpha and beta
///
/// Coefficient alpha and beta evolve in time using computed derivatives for each particle separately.
/// Although the same mechanism could be used with any artificial viscosity, the current implementation is
/// only an extension of the standard scalar artificial viscosity.
class MorrisMonaghanAV : public IEquationTerm {
public:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<const Float> alpha, beta, dalpha, cs, rho, m;
        ArrayView<const Vector> r, v;
        ArrayView<Vector> dv;
        ArrayView<Float> du;
        const Float eps = 1.e-2_f;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            tie(alpha, dalpha) = input.getAll<Float>(QuantityId::AV_ALPHA);
            beta = input.getValue<Float>(QuantityId::AV_BETA);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);
            rho = input.getValue<Float>(QuantityId::DENSITY);
            m = input.getValue<Float>(QuantityId::MASS);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        template <bool TSymmetric>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Float Pi = operator()(i, j);
                const Float heating = 0.5_f * Pi * dot(v[i] - v[j], grads[k]);

                dv[i] += m[j] * Pi * grads[k];
                du[i] += m[j] * heating;

                if (TSymmetric) {
                    dv[j] -= m[i] * Pi * grads[k];
                    du[j] += m[i] * heating;
                }
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
            const Float mu = hbar * dvdr / (getSqrLength(r[i] - r[j]) + eps * sqr(hbar));
            return 1._f / rhobar * (-alphabar * csbar * mu + betabar * sqr(mu));
        }
    };

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<Derivative>(settings);
        derivatives.require<VelocityDivergence<NoGradientCorrection>>(settings);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        ArrayView<Float> alpha = storage.getValue<Float>(QuantityId::AV_ALPHA);
        ArrayView<Float> beta = storage.getValue<Float>(QuantityId::AV_BETA);
        // always keep beta = 2*alpha
        for (Size i = 0; i < alpha.size(); ++i) {
            beta[i] = 2._f * alpha[i];
        }
    }

    virtual void finalize(Storage& storage) override {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Float> alpha, dalpha;
        tie(alpha, dalpha) = storage.getAll<Float>(QuantityId::AV_ALPHA);
        ArrayView<Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
        ArrayView<Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
        constexpr Float eps = 0.1_f;
        for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
            MaterialView material = storage.getMaterial(matIdx);
            const Interval bounds = material->getParam<Interval>(BodySettingsId::AV_ALPHA_RANGE);
            for (Size i : material.sequence()) {
                const Float tau = r[i][H] / (eps * cs[i]);
                ASSERT(tau > 0.f);
                const Float decayTerm = -(alpha[i] - Float(bounds.lower())) / tau;
                const Float sourceTerm = max(-(Float(bounds.upper()) - alpha[i]) * divv[i], 0._f);
                dalpha[i] = decayTerm + sourceTerm;
                ASSERT(isReal(dalpha[i]));
            }
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        storage.insert<Float>(
            QuantityId::AV_ALPHA, OrderEnum::FIRST, material.getParam<Float>(BodySettingsId::AV_ALPHA));
        /// \todo maybe remove AV_BETA as we are using AV_BETA = 2*AV_ALPHA anyway
        storage.insert<Float>(
            QuantityId::AV_BETA, OrderEnum::ZERO, material.getParam<Float>(BodySettingsId::AV_BETA));

        storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
        const Interval avRange = material.getParam<Interval>(BodySettingsId::AV_ALPHA_RANGE);
        material.setRange(QuantityId::AV_ALPHA, avRange, 0._f);
    }
};

NAMESPACE_SPH_END
