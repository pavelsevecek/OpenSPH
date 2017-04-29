#pragma once

/// Implementation of the Balsara switch (Balsara, 1995), designed to reduce artificial viscosity in shear
/// flows and avoid numerical issues, such as unphysical transport of angular momentum.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


/// Balsara switch is a template, needs another artificial viscosity as a template parameter. The template
/// parameter shall be an EquationTerm; Balsara switch then forward all functions (initialize, finalize, ...)
/// to this base AV. Furthermore, the AV must define a class Derivative with operator()(i, j), returing value
/// Pi_ij of the artificial viscosity between particles i and j.
template <typename AV>
class BalsaraSwitch : public Abstract::EquationTerm {
    class Derivative : public Abstract::Derivative {
    private:
        ArrayView<const Float> m;
        ArrayView<const Float> cs;
        ArrayView<const Vector> r, v;
        ArrayView<const Float> divv;
        ArrayView<const Vector> rotv;
        ArrayView<Vector> dv;
        ArrayView<Float> du;
        typename AV::Derivative av;
        const Float eps = 1.e-4_f;

    public:
        virtual void initialize(const Storage& input, Accumulated& results) {
            m = input.getValue<Float>(QuantityId::MASSES);
            ArrayView<const Vector> dummy;
            tie(r, v, dv) = input.getAll<Vector>(QuantityId::POSITIONS);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);
            divv = input.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
            rotv = input.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
            dv = results.getValue<Vector>(QuantityId::POSITIONS);
            du = results.getValue<Float>(QuantityId::ENERGY);
            av.template initialize(input, results);
        }

        virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Float Pi = 0.5_f * (factor(i) + factor(j)) * av(i, j);
                ASSERT(isReal(Pi));
                dv[i] += m[j] * Pi * grads[k];
                dv[j] -= m[i] * Pi * grads[k];

                const Float heating = 0.5_f * Pi * dot(v[i] - v[j], grads[k]);
                du[i] += m[j] * heating;
                du[j] += m[i] * heating;
            }
        }

        INLINE Float factor(const Size i) {
            const Float dv = abs(divv[i]);
            const Float rv = getLength(rotv[i]);
            return dv / (dv + rv + eps * cs[i] / r[i][H]);
        }
    };

    AV av;
    bool storeFactor;

public:
    BalsaraSwitch(const RunSettings& settings)
        : av(settings) {
        storeFactor = settings.get<bool>(RunSettingsId::MODEL_AV_BALSARA_STORE);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives) override {
        derivatives.require<VelocityDivergence>();
        derivatives.require<VelocityRotation>();
        derivatives.require<Derivative>();
    }

    virtual void initialize(Storage& storage) override {
        av.initialize(storage);
    }

    virtual void finalize(Storage& storage) override {
        av.finalize(storage);
        if (storeFactor) {
            Accumulated dummy;
            Derivative derivative;
            derivative.initialize(storage, dummy);
            ArrayView<Float> factor = storage.getValue<Float>(QuantityId::AV_BALSARA);
            for (Size i = 0; i < factor.size(); ++i) {
                factor[i] = derivative.factor(i);
            }
        }
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
        storage.insert(QuantityId::VELOCITY_ROTATION, OrderEnum::ZERO, Vector(0._f));
        if (storeFactor) {
            storage.insert(QuantityId::AV_BALSARA, OrderEnum::ZERO, 0._f);
        }
        av.create(storage, material);
    }
};

NAMESPACE_SPH_END
