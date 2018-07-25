#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// Straighforward implementation of viscous force, leads to high friction of boundary and does not conserve
/// angular momentum.
class NaiveViscosity : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<const Vector> r, v;
        ArrayView<const Float> m, rho;
        ArrayView<Vector> dv;

        /// Helper quantities for visualizations
        ArrayView<Vector> divGradV;
        ArrayView<Vector> gradDivV;

        /// \todo possibly generalize if we need multiple materials with different viscosities
        Float eta;
        Float zeta;

        static constexpr Float multiplier = 1.e17_f;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED) {}

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
            results.insert<Vector>(QuantityId::VELOCITY_LAPLACIAN, OrderEnum::ZERO, BufferSource::UNIQUE);
            results.insert<Vector>(
                QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, OrderEnum::ZERO, BufferSource::UNIQUE);
        }

        virtual bool equals(const IDerivative& other) const override {
            if (!DerivativeTemplate<Derivative>::equals(other)) {
                return false;
            }
            const Derivative* actOther = assert_cast<const Derivative*>(&other);
            return eta == actOther->eta && zeta == actOther->zeta;
        }

        INLINE void init(const Storage& input, Accumulated& results) {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            gradDivV =
                results.getBuffer<Vector>(QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, OrderEnum::ZERO);
            divGradV = results.getBuffer<Vector>(QuantityId::VELOCITY_LAPLACIAN, OrderEnum::ZERO);

            MaterialView mat0 = input.getMaterial(0);
            eta = mat0->getParam<Float>(BodySettingsId::SHEAR_VISCOSITY);
            zeta = mat0->getParam<Float>(BodySettingsId::BULK_VISCOSITY);
            ASSERT(input.isHomogeneous(BodySettingsId::SHEAR_VISCOSITY));
            ASSERT(input.isHomogeneous(BodySettingsId::BULK_VISCOSITY));
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Vector dgv = laplacian(v[j] - v[i], grad, r[j] - r[i]);
            const Vector gdv = gradientOfDivergence(v[j] - v[i], grad, r[j] - r[i]);
            const Vector shear = eta * (dgv + gdv / 3._f);
            const Vector iso = zeta * gdv;
            const Vector term = (shear + iso) / (rho[i] * rho[j]);
            dv[i] += m[j] * term;
            gradDivV[i] += m[j] / rho[j] * gdv * multiplier;
            divGradV[i] += m[j] / rho[j] * dgv * multiplier;
            if (Symmetrize) {
                dv[j] -= m[i] * term;
                gradDivV[j] -= m[i] / rho[i] * gdv * multiplier;
                divGradV[j] -= m[i] / rho[i] * dgv * multiplier;
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::VELOCITY_LAPLACIAN, OrderEnum::ZERO, Vector(0._f));
        storage.insert<Vector>(QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, OrderEnum::ZERO, Vector(0._f));
    }
};

/// Flebbe et al. (1994)
class ViscousStress : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
    private:
        ArrayView<const Float> m, rho;
        ArrayView<const SymmetricTensor> gradV;

        ArrayView<Vector> dv;
        ArrayView<Vector> frict;

        Float eta;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings) {}

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
            results.insert<Vector>(QuantityId::FRICTION, OrderEnum::ZERO, BufferSource::UNIQUE);
        }

        virtual bool equals(const IDerivative& other) const override {
            if (!DerivativeTemplate<Derivative>::equals(other)) {
                return false;
            }
            const Derivative* actOther = assert_cast<const Derivative*>(&other);
            return eta == actOther->eta;
        }

        INLINE void init(const Storage& input, Accumulated& results) {
            tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
            gradV = input.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
            frict = results.getBuffer<Vector>(QuantityId::FRICTION, OrderEnum::ZERO);

            eta = input.getMaterial(0)->getParam<Float>(BodySettingsId::SHEAR_VISCOSITY);
            ASSERT(input.isHomogeneous(BodySettingsId::SHEAR_VISCOSITY));
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            SymmetricTensor sigmai =
                2._f * gradV[i] - 2._f / 3._f * gradV[i].trace() * SymmetricTensor::identity();
            SymmetricTensor sigmaj =
                2._f * gradV[j] - 2._f / 3._f * gradV[j].trace() * SymmetricTensor::identity();

            const Vector f = eta * (sigmai / pow<2>(rho[i]) + sigmaj / pow<2>(rho[j])) * grad;
            dv[i] += m[j] * f;
            frict[i] += m[j] * f;
            if (Symmetrize) {
                dv[j] -= m[i] * f;
                frict[j] -= m[i] * f;
            }
        }
    };


public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        const Flags<DerivativeFlag> flags = DerivativeFlag::SUM_ONLY_UNDAMAGED;
        derivatives.require(makeDerivative<VelocityGradient>(settings, flags));
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        storage.insert<Vector>(QuantityId::FRICTION, OrderEnum::ZERO, Vector(0._f));
        storage.insert<SymmetricTensor>(
            QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    }
};

class SimpleDamping : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
        ArrayView<const Vector> r, v;
        ArrayView<const Float> cs;
        ArrayView<Vector> dv;

        Float k;

    public:
        explicit Derivative(const RunSettings& settings)
            : DerivativeTemplate<Derivative>(settings) {}

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
        }

        virtual bool equals(const IDerivative& other) const override {
            if (!DerivativeTemplate<Derivative>::equals(other)) {
                return false;
            }
            const Derivative* actOther = assert_cast<const Derivative*>(&other);
            return k == actOther->k;
        }

        INLINE void init(const Storage& input, Accumulated& results) {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);

            k = input.getMaterial(0)->getParam<Float>(BodySettingsId::DAMPING_COEFFICIENT);
            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& UNUSED(grad)) {
            const Float csbar = 0.5_f * (cs[i] + cs[j]);
            const Vector f = k * (v[i] - v[j]) / csbar;
            dv[i] -= f;
            if (Symmetrize) {
                dv[j] += f;
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
