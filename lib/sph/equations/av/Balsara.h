#pragma once

/// \file Balsara.h
/// \brief Implementation of the Balsara switch
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/equations/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Implementation of the Balsara switch \cite Balsara_1995, designed to reduce artificial viscosity in
/// shear flows and avoid numerical issues, such as unphysical transport of angular momentum.

/// Balsara switch is a template, needs another artificial viscosity as a template parameter. The template
/// parameter shall be an EquationTerm; Balsara switch then forward all functions (initialize, finalize, ...)
/// to this base AV. Furthermore, the AV must define a class Derivative with operator()(i, j), preferably
/// force inlined, returing value Pi_ij of the artificial viscosity between particles i and j.
template <typename AV>
class BalsaraSwitch : public Abstract::EquationTerm {
    class Derivative : public DerivativeTemplate<Derivative> {
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
        Derivative(const RunSettings& settings)
            : av(settings) {}

        virtual void create(Accumulated& results) override {
            results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
        }

        virtual void initialize(const Storage& input, Accumulated& results) {
            m = input.getValue<Float>(QuantityId::MASSES);
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITIONS);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);
            divv = input.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
            rotv = input.getValue<Vector>(QuantityId::VELOCITY_ROTATION);

            dv = results.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
            du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
            av.template initialize(input, results);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                const Float Pi = 0.5_f * (factor(i) + factor(j)) * av(i, j);
                ASSERT(isReal(Pi));
                dv[i] += m[j] * Pi * grads[k];
                const Float heating = 0.5_f * Pi * dot(v[i] - v[j], grads[k]);
                du[i] += m[j] * heating;

                if (Symmetrize) {
                    dv[j] -= m[i] * Pi * grads[k];
                    du[j] += m[i] * heating;
                }
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
        : av(Detail::DerivativeTraits<AV>::make(settings)) {
        storeFactor = settings.get<bool>(RunSettingsId::SPH_AV_BALSARA_STORE);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<VelocityDivergence>(settings);
        derivatives.require<VelocityRotation>(settings);
        derivatives.require<Derivative>(settings);
    }

    virtual void initialize(Storage& storage) override {
        av.initialize(storage);
    }

    virtual void finalize(Storage& storage) override {
        av.finalize(storage);
        if (storeFactor) {
            Accumulated dummy;
            /// \todo is this ok?
            Derivative derivative(RunSettings::getDefaults());
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
