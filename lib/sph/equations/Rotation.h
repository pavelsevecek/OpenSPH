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
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            if (flag[i] != flag[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                continue;
            }
            const TracelessTensor t = (s[i] + s[j]) / (rho[i] * rho[j]);
            const Vector force = t * grads[k];
            const Vector torque = 0.5_f * cross(r[j] - r[i], force);
            ASSERT(isReal(force) && isReal(torque));
            domega[i] += m[i] / I[i] * m[j] * torque;
            ASSERT(isReal(domega[i]));

            if (Symmetrize) {
                domega[j] += m[j] / I[j] * m[i] * torque;
                ASSERT(isReal(domega[j]));
            }
        }
    }
};


class RotationStrengthVelocityGradient : public DerivativeTemplate<RotationStrengthVelocityGradient> {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> r, omega;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    ArrayView<SymmetricTensor> gradv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
        tie(r, omega) = input.getValues<Vector>(QuantityId::POSITION, QuantityId::ANGULAR_VELOCITY);
        idxs = input.getValue<Size>(QuantityId::FLAG);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);

        gradv = results.getBuffer<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                continue;
            }
            const Vector dr = r[j] - r[i];
            const Vector dvj = cross(omega[j], dr);
            const SymmetricTensor tj = outer(dvj, grads[k]);
            ASSERT(isReal(tj));
            gradv[i] -= m[j] / rho[j] * tj;
            if (Symmetrize) {
                const Vector dvi = cross(omega[i], dr);
                const SymmetricTensor ti = outer(dvi, grads[k]);
                gradv[j] -= m[i] / rho[i] * ti;
            }
        }
    }
};

class SolidStressTorque : public IEquationTerm {
private:
    /// Dimensionless moment of inertia
    ///
    /// Computed by computing integral \f[\int r^2_\perp W(r) dV \f] for selected kernel
    Float inertia;

    bool evolveAngle;

public:
    explicit SolidStressTorque(const RunSettings& settings) {
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
        derivatives.require<RotationStrengthVelocityGradient>(settings);
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

        // we can set it to here, it will be overwritten in \ref initialize anyway
        storage.insert<Float>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, 0._f);
    }
};

NAMESPACE_SPH_END
