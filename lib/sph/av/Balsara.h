#pragma once

/// Implementation of the Balsara switch (Balsara, 1995), designed to reduce artificial viscosity in shear
/// flows. Needs another artificial viscosity as a base object, Balsara switch is just a modifier.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// Balsara switch can wrap any equation term as long as they define Derivative with operator()(i, j).
template <typename AV>
class BalsaraSwitch : public Abstract::EquationTerm {
    class Derivative : public Abstract::Derivative {
    private:
        ArrayView<const Float> m;
        ArrayView<const Float> cs;
        ArrayView<const Vector> r;
        ArrayView<const Float> divv;
        ArrayView<const Vector> rotv;
        ArrayView<Vector> dv;
        typename AV::Derivative av;
        const Float eps = 1.e-4_f;

    public:
        virtual void initialize(const Storage& input, Accumulated& results) {
            m = input.getValue<Float>(QuantityId::MASSES);
            r = input.getValue<Vector>(QuantityId::POSITIONS);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);
            divv = input.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
            rotv = input.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
            dv = results.getValue<Vector>(QuantityId::POSITIONS);
            av.template initialize(input, results);
        }

        virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Vector f = 0.5_f * (factor(i) + factor(j)) * av(i, j) * grads[k];
                ASSERT(isReal(f));
                dv[i] += m[j] * f;
                dv[j] -= m[i] * f;
                TODO("Add heating");
            }
        }

    private:
        INLINE Float factor(const Size i) {
            const Float dv = abs(divv[i]);
            const Float rv = getLength(rotv[i]);
            return dv / (dv + rv + eps * cs[i] / r[i][H]);
        }
    };

    AV av;

public:
    virtual void setDerivatives(DerivativeHolder& derivatives) override {
        derivatives.require<VelocityDivergence>();
        derivatives.require<VelocityRotation>();
        av.setDerivatives(derivatives);
    }

    virtual void finalize(Storage& storage) override {
        av.finalize(storage);
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        av.create(storage, material);
    }
};

NAMESPACE_SPH_END
