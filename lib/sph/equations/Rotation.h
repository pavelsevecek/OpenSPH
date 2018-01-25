#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// \todo heavy duplication of stress derivative
class RotationStressDivergence : public DerivativeTemplate<RotationStressDivergence> {
private:
    ArrayView<const Float> rho, m, I;
    ArrayView<const TracelessTensor> s;
    ArrayView<const Float> reduce;
    ArrayView<const Size> flag;
    ArrayView<const Vector> r;
    ArrayView<Vector> domega;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m, I) =
            input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS, QuantityId::MOMENT_OF_INERTIA);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        flag = input.getValue<Size>(QuantityId::FLAG);
        r = input.getValue<Vector>(QuantityId::POSITION);
        domega = results.getBuffer<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST);
    }


    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        if (flag[i] != flag[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const TracelessTensor t = (s[i] + s[j]) / (rho[i] * rho[j]);
        const Vector force = t * grad;
        const Vector torque = 0.5_f * cross(r[j] - r[i], force);
        ASSERT(isReal(force) && isReal(torque));
        domega[i] += m[i] / I[i] * m[j] * torque;
        ASSERT(isReal(domega[i]));

        if (Symmetrize) {
            domega[j] += m[j] / I[j] * m[i] * torque;
            ASSERT(isReal(domega[j]));
        }
    }
};


template <QuantityId Id, typename TDerived>
class RotationStrengthVelocityTemplate : public SymmetricDerivative {
protected:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> r, omega;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    ArrayView<SymmetricTensor> gradv;

    bool useCorrectionTensor;
    ArrayView<const SymmetricTensor> C;

public:
    explicit RotationStrengthVelocityTemplate(const RunSettings& settings) {
        useCorrectionTensor = settings.get<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR);
    }

    virtual void create(Accumulated& results) override {
        results.insert<SymmetricTensor>(Id, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
        tie(r, omega) = input.getValues<Vector>(QuantityId::POSITION, QuantityId::ANGULAR_VELOCITY);
        idxs = input.getValue<Size>(QuantityId::FLAG);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);

        gradv = results.getBuffer<SymmetricTensor>(Id, OrderEnum::ZERO);
        if (useCorrectionTensor) {
            // RotationStrengthVelocityGradient can only be used with StrengthVelocityGradient, that computes
            // the correction tensor and saves it to the accumulated buffer. We don't compute it here, it must
            // be already computed.
            C = results.getBuffer<SymmetricTensor>(
                QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
        }
    }

    virtual void evalNeighs(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        if (useCorrectionTensor) {
            for (Size k = 0; k < neighs.size(); ++k) {
                ASSERT(C[idx] != SymmetricTensor::null()); // check that it has been computed
                static_cast<TDerived*>(this)->template eval<false>(idx, neighs[k], C[idx] * grads[k]);
            }
        } else {
            for (Size k = 0; k < neighs.size(); ++k) {
                static_cast<TDerived*>(this)->template eval<false>(idx, neighs[k], grads[k]);
            }
        }
    }

    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        ASSERT(!useCorrectionTensor);
        for (Size k = 0; k < neighs.size(); ++k) {
            static_cast<TDerived*>(this)->template eval<true>(idx, neighs[k], grads[k]);
        }
    }
};

class RotationStrengthVelocityGradient
    : public RotationStrengthVelocityTemplate<QuantityId::STRENGTH_VELOCITY_GRADIENT,
          RotationStrengthVelocityGradient> {

public:
    using RotationStrengthVelocityTemplate<QuantityId::STRENGTH_VELOCITY_GRADIENT,
        RotationStrengthVelocityGradient>::RotationStrengthVelocityTemplate;

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const Vector dr = r[j] - r[i];
        const Vector dvj = cross(omega[j], dr);
        const SymmetricTensor tj = outer(dvj, grad);
        ASSERT(isReal(tj));
        gradv[i] -= m[j] / rho[j] * tj;
        if (Symmetrize) {
            const Vector dvi = cross(omega[i], dr);
            const SymmetricTensor ti = outer(dvi, grad);
            gradv[j] -= m[i] / rho[i] * ti;
        }
    }
};

class RotationStrengthDensityVelocityGradient
    : public RotationStrengthVelocityTemplate<QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT,
          RotationStrengthDensityVelocityGradient> {

public:
    using RotationStrengthVelocityTemplate<QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT,
        RotationStrengthDensityVelocityGradient>::RotationStrengthVelocityTemplate;

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const Vector dr = r[j] - r[i];
        const Vector dvj = cross(omega[j], dr);
        const SymmetricTensor tj = outer(dvj, grad);
        ASSERT(isReal(tj));
        gradv[i] -= m[j] * tj;
        if (Symmetrize) {
            const Vector dvi = cross(omega[i], dr);
            const SymmetricTensor ti = outer(dvi, grad);
            gradv[j] -= m[i] * ti;
        }
    }
};

class BenzAsphaugSolidStressTorque : public IEquationTerm {
private:
    /// Dimensionless moment of inertia
    ///
    /// Computed by computing integral \f[\int r^2_\perp W(r) dV \f] for selected kernel
    Float inertia;

    bool evolveAngle;

public:
    explicit BenzAsphaugSolidStressTorque(const RunSettings& settings) {
        const KernelEnum kernel = settings.get<KernelEnum>(RunSettingsId::SPH_KERNEL);
        switch (kernel) {
        case KernelEnum::CUBIC_SPLINE:
            inertia = 0.6_f;
            break;
        default:
            NOT_IMPLEMENTED;
        }

        evolveAngle = settings.get<bool>(RunSettingsId::SPH_PHASE_ANGLE);
    }

    /// \brief Returns the dimensionless moment of inertia of particles.
    ///
    /// Exposed mainly for testing purposes.
    INLINE Float getInertia() const {
        return inertia;
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<RotationStressDivergence>(settings);

        FormulationEnum formulation = settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION);
        // these derivative are never actually used by the equation term, but they accumulate additional
        // values to the velocity gradient, used by other terms in the selected SPH formulation.
        switch (formulation) {
        case FormulationEnum::BENZ_ASPHAUG:
            derivatives.require<RotationStrengthVelocityGradient>(settings);
            break;
        case FormulationEnum::STANDARD:
            derivatives.require<RotationStrengthVelocityGradient>(settings);
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<Float> I = storage.getValue<Float>(QuantityId::MOMENT_OF_INERTIA);
        for (Size i = 0; i < r.size(); ++i) {
            I[i] = inertia * m[i] * sqr(r[i][H]);
            ASSERT(isReal(I[i]) && I[i] > EPS);
        }
    }

    virtual void finalize(Storage& storage) override {
        if (evolveAngle) {
            // copy angular velocity into the phase angle derivative (they are separate quantities)
            ArrayView<Vector> dphi = storage.getDt<Vector>(QuantityId::PHASE_ANGLE);
            ArrayView<Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
            for (Size i = 0; i < omega.size(); ++i) {
                dphi[i] = omega[i];
                ASSERT(maxElement(abs(omega[i])) < 1.e6_f, omega[i]);
            }
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        // although we should evolve phase angle as second-order quantity rather than angular velocity, the
        // SPH particles are spherically symmetric, so there is no point of doing that
        storage.insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST, Vector(0._f));

        // let the angular velocity be unbounded and not affecting timestepping
        material.setRange(QuantityId::ANGULAR_VELOCITY, Interval::unbounded(), LARGE);

        // if needed (for testing purposes), evolve phase angle as a separate first order quantity; this is a
        // waste of one buffer as we need to copy angular velocities between quantities, but it makes no sense
        // to use in final runs anyway.
        if (evolveAngle) {
            storage.insert<Vector>(QuantityId::PHASE_ANGLE, OrderEnum::FIRST, Vector(0._f));
            material.setRange(QuantityId::PHASE_ANGLE, Interval::unbounded(), LARGE);
        }

        // we can set it to zero here, it will be overwritten in \ref initialize anyway
        storage.insert<Float>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, 0._f);
    }
};

NAMESPACE_SPH_END
