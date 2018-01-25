#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN


class InternalFriction : public IEquationTerm {
private:
    class VelocityLaplacian : public DerivativeTemplate<VelocityLaplacian> {
    private:
        ArrayView<const Vector> r, v;
        ArrayView<const Float> m, rho;
        ArrayView<Vector> dv;

        Float nu;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            tie(m, rho) = input.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);
            nu = input.getMaterial(0)->getParam<Float>(BodySettingsId::KINEMATIC_VISCOSITY);

            dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, const Size j, const Vector& grad) {
            const Vector delta = laplacian(v[j] - v[i], grad, r[j] - r[i]);
            dv[i] -= m[j] / rho[j] * nu * delta;
            if (Symmetrize) {
                dv[j] += m[i] / rho[i] * nu * delta;
            }
        }
    };

public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<VelocityLaplacian>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


class SimpleDamping : public IEquationTerm {
private:
    class Derivative : public DerivativeTemplate<Derivative> {
        ArrayView<const Vector> r, v;
        ArrayView<const Float> cs;
        ArrayView<Vector> dv;

        Float k;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);

            /// \todo different coefficient - damping coeff
            k = input.getMaterial(0)->getParam<Float>(BodySettingsId::KINEMATIC_VISCOSITY);
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
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
