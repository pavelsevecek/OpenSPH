#pragma once

/// \file Conductivity.h
/// \brief Artificial thermal conductivity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

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

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings) {
            alpha = settings.get<Float>(RunSettingsId::SPH_AC_ALPHA);
            beta = settings.get<Float>(RunSettingsId::SPH_AC_BETA);
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
            return alpha == other.alpha && beta == other.beta;
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Vector e = (r[i] - r[j]) / getLength(r[i] - r[j]);
            const Float rho_bar = 0.5_f * (rho[i] + rho[j]);
            const Float vu_sig = sgn((p[i] - p[j]) * (u[i] - u[j])) * sqrt(abs(p[i] - p[j]) / rho_bar);
            const Float v_sig = cs[i] + cs[j] - beta * dot(v[i] - v[j], e);
            const Float a1 = alpha * vu_sig * (u[i] - u[j]);
            const Float a2 = -0.5_f * alpha * v_sig * sqr(dot(v[i] - v[j], e));
            const Float sum = (a1 + a2) * dot(e, grad);
            du[i] += m[j] / rho_bar * sum;

            if (Symmetrize) {
                du[j] += m[j] / rho_bar * sum;
            }
        }
    };

    ArtificialConductivity(const RunSettings& settings) {
        const Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
        if (forces != ForceEnum::PRESSURE) {
            throw InvalidSetup(
                "Artificiacl conductivity cannot be used with forces other than pressure gradient.");
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