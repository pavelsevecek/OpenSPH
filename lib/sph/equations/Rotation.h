#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// \todo heavy duplication of stress derivative
class RotationDerivative : public DerivativeTemplate<RotationDerivative> {
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
            input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES, QuantityId::MOMENT_OF_INERTIA);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        flag = input.getValue<Size>(QuantityId::FLAG);
        r = input.getValue<Vector>(QuantityId::POSITIONS);
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

            if (Symmetrize) {
                domega[i] += m[j] / I[j] * m[i] * torque;
            }
        }
    }
};


class Rotation : public IEquationTerm {
private:
    /// Dimensionless moment of inertia
    ///
    /// Computed by computing integral \f[\int r^2_\perp W(r) dV \f] for selected kernel
    Float inertia;

public:
    explicit Rotation(const RunSettings& settings) {
        const KernelEnum kernel = settings.get<KernelEnum>(RunSettingsId::SPH_KERNEL);
        switch (kernel) {
        case KernelEnum::CUBIC_SPLINE:
            inertia = 0.02_f; // 0.6
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<RotationDerivative>(settings);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
        ArrayView<Float> I = storage.getValue<Float>(QuantityId::MOMENT_OF_INERTIA);
        for (Size i = 0; i < r.size(); ++i) {
            I[i] = inertia * m[i] * sqr(r[i][H]);
        }
    }

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& material) const override {
        // although we should evolve phase angle as second-order quantity rather than angular velocity, the
        // SPH particles are spherically symmetric, so there is no point of doing that
        storage.insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST, Vector(0._f));

        // let the angular velocity be unbounded and not affecting timestepping
        material.setRange(QuantityId::ANGULAR_VELOCITY, Interval::unbounded(), LARGE);

        // we can set it to here, it will be overwritten in \ref initialize anyway
        storage.insert<Float>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, 0._f);
    }
};

NAMESPACE_SPH_END
