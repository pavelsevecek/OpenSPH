#pragma once

/// \file DeltaSph.h
/// \brief Delta-SPH modification of the standard SPH formulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

namespace DeltaSph {

class RenormalizedDensityGradient : public DerivativeTemplate<RenormalizedDensityGradient> {
private:
    ArrayView<const Float> rho;
    ArrayView<Vector> drho;

public:
    explicit RenormalizedDensityGradient(const RunSettings& settings)
        : DerivativeTemplate<Derivative>(settings,
              DerivativeFlag::SUM_ONLY_UNDAMAGED | DerivativeFlag::CORRECTED) {}

    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::DELTASPH_DENSITY_GRADIENT, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void init(const Storage& input, Accumulated& results) {
        drho = results.getBuffer<Vector>(QuantityId::DELTASPH_DENSITY_GRADIENT, OrderEnum::ZERO);
        rho = input.getValue<Float>(QuantityId::DENSITY);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        ASSERT(Symmetrize == false);
        const Vector f = (rho[j] - rho[i]) * grad;
        drho[i] += m[j] / rho[j] * f;
        if (Symmetrize) {
            drho[j] += m[i] / rho[i] * f;
        }
    }
};

/// \brief Numerical diffusion of density
///
/// See Marrone et at 2011. delta-SPH model for simulating violent impact flows
class DensityDiffustion : public IEquationTerm {
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<Float> drho;
        ArrayView<const Vector> r, gradRho;
        ArrayView<const Float> m, rho, cs;

        Float delta;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED) {
            delta = settings.get<Float>(RunSettingsId::SPH_DENSITY_DIFFUSION_DELTA);
        }

        virtual void create(Accumulated& results) override {
            results.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, BufferSource::SHARED);
        }

        virtual bool equals(const IDerivative& other) const override {
            if (!DerivativeTemplate<Derivative>::equals(other)) {
                return false;
            }
            const Derivative* actOther = assert_cast<const Derivative*>(&other);
            return delta == actOther->delta;
        }

        INLINE void init(const Storage& input, Accumulated& results) {
            tie(r, gradRho) =
                input.getValues<Vector>(QuantityId::POSITION, QuantityId::DELTASPH_DENSITY_GRADIENT);
            tie(m, cs) = input.getValues<Float>(QuantityId::MASS, QuantityId::SOUND_SPEED);
            tie(rho, drho) = input.getAll<Float>(QuantityId::DENSITY);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Vector dr = r[j] - r[i];
            const Vector psi = 2._f * (rho[j] - rho[i]) * dr / getSqrLength(dr) - (gradRho[i] + gradRho[j]);
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            const Float cbar = 0.5_f * (cs[i] + cs[j]);
            const Float f = delta * hbar * cbar * dot(psi, grad);

            drho[i] += m[j] / rho[j] * f;
            if (Symmetrize) {
                TODO("check sign!");
                drho[j] += m[i] / rho[i] * f;
            }
        }
    };
};

/// \brief Numerical diffusion of velocity
class VelocityDiffusion : public IEquationTerm {
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<Vector> dv;
        ArrayView<const Vector> r, v;
        ArrayView<const Float> m, rho, cs;

        Float alpha;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED) {
            alpha = settings.get<Float>(RunSettingsId::SPH_VELOCITY_DIFFUSION_ALPHA);
        }

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
        }

        virtual bool equals(const IDerivative& other) const override {
            if (!DerivativeTemplate<Derivative>::equals(other)) {
                return false;
            }
            const Derivative* actOther = assert_cast<const Derivative*>(&other);
            return alpha == actOther->alpha;
        }

        INLINE void init(const Storage& input, Accumulated& results) {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            tie(m, rho, cs) =
                input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY, QuantityId::SOUND_SPEED);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Vector dr = r[j] - r[i];
            const Float pi = dot(v[j] - v[i], dr) / getSqrLength(dr);
            const Float hbar = 0.5_f * (h[i] + h[j]);
            /// \todo here using average values instead of const values c_0, is that OK?
            const Float cbar = 0.5_f * (cs[i] + cs[j]);
            const Vector f = alpha * hbar * cbar * pi * grad;
            dv[i] += m[j] / rho[j] * f;

            if (Symmetrize) {
                TODO("check sign!");
                dv[j] -= m[i] / rho[i] * f;
            }
        }
    };
};

} // namespace DeltaSph

NAMESPACE_SPH_END
