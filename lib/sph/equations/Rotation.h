#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

class RotationDerivative : public DerivativeTemplate<RotationDerivative> {
public:
    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            if (flag[i] != flag[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                continue;
            }
            const SymmetricTensor t = (s[i] + s[j]) / (rho[i] * rho[j]);
            const Vector force = t * grads[k];
            const Vector torque = cross(r[j] - r[i], force);
            ASSERT(isReal(force) && isReal(torque));
            dv[i] += m[j] * f;
            domega[i] += m[i] / I[i] * m[j] * torque;

            if (Symmetrize) {
                dv[j] -= m[i] * f;
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
            inertia = 0.6;
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
            I[i] = inertia * m[i] * square(r[i][H]);
        }
    }

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& storage, IMaterial& UNUSED(material)) const override {
        // although we should evolve phase angle as second-order quantity rather than angular velocity, the
        // SPH particles are spherically symmetric, so there is no point of doing that
        storage.insert<Vector>(QuantityId::ANGULAR_VELOCITY, OrderEnum::FIRST, Vector(0._f));

        // we can set it to here, it will be overwritten in \ref initialize anyway
        storage.insert<Float>(QuantityId::MOMENT_OF_INERTIA, OrderEnum::ZERO, 0._f);
    }
};

NAMESPACE_SPH_END
