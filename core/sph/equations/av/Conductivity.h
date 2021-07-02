#pragma once

/// \file Conductivity.h
/// \brief Artificial thermal conductivity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "quantities/Storage.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Artificial thermal conductivity
///
/// See Price (2008).
class ArtificialConductivity : public IEquationTerm {
public:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        Float alpha, beta;
        ArrayView<const Vector> r, v;
        ArrayView<const Float> m, rho, u, p, cs;
        ArrayView<Float> du;

        SignalSpeedEnum sig;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings) {
            alpha = settings.get<Float>(RunSettingsId::SPH_AC_ALPHA);
            beta = settings.get<Float>(RunSettingsId::SPH_AC_BETA);
            sig = settings.get<SignalSpeedEnum>(RunSettingsId::SPH_AC_SIGNAL_SPEED);
        }

        INLINE void additionalCreate(Accumulated& results) {
            results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, BufferSource::SHARED);
        }

        INLINE void additionalInitialize(const Storage& storage, Accumulated& results) {
            tie(m, rho, u, p, cs) = storage.getValues<Float>(QuantityId::MASS,
                QuantityId::DENSITY,
                QuantityId::ENERGY,
                QuantityId::PRESSURE,
                QuantityId::SOUND_SPEED);
            r = storage.getValue<Vector>(QuantityId::POSITION);
            v = storage.getDt<Vector>(QuantityId::POSITION);
            du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        INLINE bool additionalEquals(const Derivative& other) const {
            return alpha == other.alpha && beta == other.beta && sig == other.sig;
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Float eps = 1.e-6_f;
            const Vector e = (r[i] - r[j]) / (getLength(r[i] - r[j]) + eps);
            const Float rho_bar = 0.5_f * (rho[i] + rho[j]);
            Float vu_sig;
            if (sig == SignalSpeedEnum::PRESSURE_DIFFERENCE) {
                vu_sig = sgn((p[i] - p[j]) * (u[i] - u[j])) * sqrt(abs(p[i] - p[j]) / rho_bar);
            } else {
                SPH_ASSERT(sig == SignalSpeedEnum::VELOCITY_DIFFERENCE);
                vu_sig = abs(dot(v[i] - v[j], e));
            }
            const Float a = alpha * vu_sig * (u[i] - u[j]);
            const Float heat = a * dot(e, grad) / rho_bar;
            du[i] += m[j] * heat;

            if (Symmetrize) {
                du[j] -= m[j] * heat;
            }
        }
    };

    ArtificialConductivity(const RunSettings& settings) {
        const SignalSpeedEnum sig = settings.get<SignalSpeedEnum>(RunSettingsId::SPH_AC_SIGNAL_SPEED);
        const Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
        if (sig == SignalSpeedEnum::PRESSURE_DIFFERENCE && forces != ForceEnum::PRESSURE) {
            throw InvalidSetup(
                "Artificial conductivity with pressure-based signal speed cannot be used with forces other "
                "than pressure gradient. Consider using the velocity-based signal speed instead.");
        }
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
