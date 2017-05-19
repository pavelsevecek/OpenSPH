#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN


class InternalFriction : public Abstract::EquationTerm {
private:
    class VelocityLaplacian : public DerivativeTemplate<VelocityLaplacian> {
    private:
        ArrayView<const Vector> r, v;
        ArrayView<const Float> m, rho;
        ArrayView<Vector> dv;

        Float nu;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITIONS);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITIONS);
            tie(m, rho) = input.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);
            nu = input.getMaterial(0)->getParam<Float>(BodySettingsId::KINEMATIC_VISCOSITY);
            dv = results.getValue<Vector>(QuantityId::POSITIONS);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Vector delta = laplacian(v[j] - v[i], grads[k], r[j] - r[i]);
                dv[i] -= m[j] / rho[j] * nu * delta;
                if (Symmetrize) {
                    dv[j] += m[i] / rho[i] * nu * delta;
                }
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<VelocityLaplacian>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
