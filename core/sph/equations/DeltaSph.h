#pragma once

/// \file DeltaSph.h
/// \brief Delta-SPH modification of the standard SPH formulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

namespace DeltaSph {

class RenormalizedDensityGradient : public DerivativeTemplate<RenormalizedDensityGradient> {
private:
    ArrayView<const Float> m;
    ArrayView<const Float> rho;
    ArrayView<Vector> drho;

public:
    explicit RenormalizedDensityGradient(const RunSettings& settings)
        : DerivativeTemplate<RenormalizedDensityGradient>(settings,
              DerivativeFlag::SUM_ONLY_UNDAMAGED | DerivativeFlag::CORRECTED) {}

    void additionalCreate(Accumulated& results) {
        results.insert<Vector>(QuantityId::DELTASPH_DENSITY_GRADIENT, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        drho = results.getBuffer<Vector>(QuantityId::DELTASPH_DENSITY_GRADIENT, OrderEnum::ZERO);
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
    }

    INLINE bool additionalEquals(const RenormalizedDensityGradient& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        SPH_ASSERT(Symmetrize == false);
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
class DensityDiffusion : public IEquationTerm {
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

        void additionalCreate(Accumulated& results) {
            results.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, BufferSource::SHARED);
        }

        bool additionalEquals(const Derivative& other) const {
            return delta == other.delta;
        }

        INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
            tie(r, gradRho) =
                input.getValues<Vector>(QuantityId::POSITION, QuantityId::DELTASPH_DENSITY_GRADIENT);
            tie(m, rho, cs) =
                input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY, QuantityId::SOUND_SPEED);
            drho = results.getBuffer<Float>(QuantityId::DENSITY, OrderEnum::FIRST);
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

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<RenormalizedDensityGradient>(settings));
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::DELTASPH_DENSITY_GRADIENT, OrderEnum::ZERO, Vector(0._f));
    }
};

/// \brief Numerical diffusion of velocity
class VelocityDiffusion : public IEquationTerm {
private:
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

        void additionalCreate(Accumulated& results) {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
        }

        bool additionalEquals(const Derivative& other) const {
            return alpha == other.alpha;
        }

        INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
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
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            /// \todo here using average values instead of const values c_0, is that OK?
            const Float cbar = 0.5_f * (cs[i] + cs[j]);
            const Vector f = alpha * hbar * cbar * pi * grad;
            const Float dh = dv[i][H];
            dv[i] += m[j] / rho[j] * f;
            dv[i][H] = dh;

            if (Symmetrize) {
                TODO("check sign!");
                dv[j] -= m[i] / rho[i] * f;
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::DELTASPH_DENSITY_GRADIENT, OrderEnum::ZERO, Vector(0._f));
    }
};

} // namespace DeltaSph

NAMESPACE_SPH_END
