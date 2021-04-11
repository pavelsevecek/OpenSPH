#include "sph/equations/Rotation.h"

NAMESPACE_SPH_BEGIN

/// \brief Returns true if given particles interacts due to material strength.
///
/// This happens if both particles belong to the same body or they have nonzero damage and yielding
/// reduction.
/*INLINE bool interacts(Size i, Size j, ArrayView<const Size> flag, ArrayView<const Float> reduce) {
    return flag[i] == flag[j] && reduce[i] > 0._f && reduce[j] > 0._f;
}

/// \todo heavy duplication of stress derivative
template <typename Term>
class RotationStressDivergence : public DerivativeTemplate<RotationStressDivergence<Term>> {
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
        if (!interacts(i, j, flag, reduce)) {
            return;
        }
        const TracelessTensor t = Term::term(s, rho, i, j);
        const Vector force = t * grad;
        const Vector torque = 0.5_f * cross(r[j] - r[i], force);
        SPH_ASSERT(isReal(force) && isReal(torque));
        domega[i] += m[i] / I[i] * m[j] * torque;
        SPH_ASSERT(isReal(domega[i]));

        if (Symmetrize) {
            domega[j] += m[j] / I[j] * m[i] * torque;
            SPH_ASSERT(isReal(domega[j]));
        }
    }
};

template <QuantityId Id, typename TDerived>
class RotationStrengthVelocityTemplate : public SymmetricDerivative {
protected:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> r, omega;
    ArrayView<const Size> flag;
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
        flag = input.getValue<Size>(QuantityId::FLAG);
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
        SPH_ASSERT(neighs.size() == grads.size());
        if (useCorrectionTensor) {
            for (Size k = 0; k < neighs.size(); ++k) {
                SPH_ASSERT(C[idx] != SymmetricTensor::null()); // check that it has been computed
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
        SPH_ASSERT(neighs.size() == grads.size());
        SPH_ASSERT(!useCorrectionTensor);
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
        // Here we modify the velocity gradient ONLY if particles interact. Equation using the velocity
        // gradient shall use it only if the particles have nonzero reduction factor, otherwise use simple
        // velocity divergence (see ContinuityEquation for example).
        if (!interacts(i, j, flag, reduce)) {
            return;
        }
        const Vector dr = r[j] - r[i];
        const Vector dvj = cross(omega[j], dr);
        const SymmetricTensor tj = outer(dvj, grad);
        SPH_ASSERT(isReal(tj));
        gradv[i] -= m[j] / rho[j] * tj;
        if (Symmetrize) {
            const Vector dvi = cross(omega[i], dr);
            const SymmetricTensor ti = outer(dvi, grad);
            gradv[j] -= m[i] / rho[i] * ti;
        }
    }
};

class RotationVelocityGradient : public RotationTemplate<QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT,
                                     RotationStrengthDensityVelocityGradient> {
public:
    using RotationStrengthVelocityTemplate<QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT,
        RotationStrengthDensityVelocityGradient>::RotationStrengthVelocityTemplate;

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        if (flag[i] != flag[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const Vector dr = r[j] - r[i];
        const Vector dvj = cross(omega[j], dr);
        const SymmetricTensor tj = outer(dvj, grad);
        SPH_ASSERT(isReal(tj));
        gradv[i] -= m[j] * tj;
        if (Symmetrize) {
            const Vector dvi = cross(omega[i], dr);
            const SymmetricTensor ti = outer(dvi, grad);
            gradv[j] -= m[i] * ti;
        }
    }
};

BenzAsphaugSph::SolidStressTorque::SolidStressTorque(const RunSettings& settings) {
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

void BenzAsphaugSph::SolidStressTorque::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    struct Term {
        INLINE static TracelessTensor term(ArrayView<const TracelessTensor> s,
            ArrayView<const Float> rho,
            const Size i,
            const Size j) {
            return BenzAsphaugSph::SolidStressForce::forceTerm(s, rho, i, j);
        }
    };
    derivatives.require<RotationStressDivergence<Term>>(settings);

    // this derivative is never actually used by the equation term, but it accumulates additional
    // values to the velocity gradient, used by other terms in the SPH formulation.
    derivatives.require<RotationStrengthVelocityGradient>(settings);
}

void BenzAsphaugSph::SolidStressTorque::initialize(Storage& storage) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Float> I = storage.getValue<Float>(QuantityId::MOMENT_OF_INERTIA);
    for (Size i = 0; i < r.size(); ++i) {
        I[i] = inertia * m[i] * sqr(r[i][H]);
        SPH_ASSERT(isReal(I[i]) && I[i] > EPS);
    }
}

void BenzAsphaugSph::SolidStressTorque::finalize(Storage& storage) {
    if (evolveAngle) {
        // copy angular velocity into the phase angle derivative (they are separate quantities)
        ArrayView<Vector> dphi = storage.getDt<Vector>(QuantityId::PHASE_ANGLE);
        ArrayView<Vector> omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
        for (Size i = 0; i < omega.size(); ++i) {
            dphi[i] = omega[i];
            SPH_ASSERT(maxElement(abs(omega[i])) < 1.e6_f, omega[i]);
        }
    }
}

void BenzAsphaugSph::SolidStressTorque::create(Storage& storage, IMaterial& material) const {
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

void StandardSph::SolidStressTorque::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    struct Term {
        INLINE static TracelessTensor term(ArrayView<const TracelessTensor> s,
            ArrayView<const Float> rho,
            const Size i,
            const Size j) {
            // namespace specified for clarity
            return StandardSph::SolidStressForce::forceTerm(s, rho, i, j);
        }
    };
    derivatives.require<RotationStressDivergence<Term>>(settings);
    derivatives.require<RotationStrengthDensityVelocityGradient>(settings);
}*/

NAMESPACE_SPH_END
