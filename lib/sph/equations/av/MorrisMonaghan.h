#pragma once

/// \file MorrisMonaghan.h
/// \brief Time-dependent artificial viscosity by Morris & Monaghan (1997).
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "sph/equations/DerivativeHelpers.h"
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
    class Derivative : public AccelerationTemplate<Derivative> {
    private:
        ArrayView<const Float> alpha, dalpha, cs, rho;
        ArrayView<const Vector> r, v;

        const Float eps = 1.e-2_f;

    public:
        explicit Derivative(const RunSettings& settings)
            : AccelerationTemplate<Derivative>(settings) {}

        INLINE void additionalCreate(Accumulated& UNUSED(results)) {}

        INLINE void additionalInitialize(const Storage& input, Accumulated& UNUSED(results)) {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            tie(alpha, dalpha) = input.getAll<Float>(QuantityId::AV_ALPHA);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);
            rho = input.getValue<Float>(QuantityId::DENSITY);
        }

        INLINE bool additionalEquals(const Derivative& UNUSED(other)) const {
            return true;
        }

        template <bool Symmetric>
        INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& grad) {
            const Float Pi = evalAv(i, j);
            const Float heating = 0.5_f * Pi * dot(v[i] - v[j], grad);

            return { Pi * grad, heating };
        }

        INLINE Float evalAv(const Size i, const Size j) const {
            const Float dvdr = dot(v[i] - v[j], r[i] - r[j]);
            if (dvdr >= 0._f) {
                return 0._f;
            }
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            const Float csbar = 0.5_f * (cs[i] + cs[j]);
            const Float rhobar = 0.5_f * (rho[i] + rho[j]);
            const Float alphabar = 0.5_f * (alpha[i] + alpha[j]);
            const Float betabar = 2._f * alphabar;
            const Float mu = hbar * dvdr / (getSqrLength(r[i] - r[j]) + eps * sqr(hbar));
            return 1._f / rhobar * (-alphabar * csbar * mu + betabar * sqr(mu));
        }
    };

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeDerivative<VelocityDivergence>(settings));
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
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
                ASSERT(tau > 0._f);
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

        storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
        const Interval avRange = material.getParam<Interval>(BodySettingsId::AV_ALPHA_RANGE);
        material.setRange(QuantityId::AV_ALPHA, avRange, 0._f);
    }
};

NAMESPACE_SPH_END
