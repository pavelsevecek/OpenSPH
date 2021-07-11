#pragma once

/// \file Balsara.h
/// \brief Implementation of the Balsara switch
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// \brief Helper function allowing to construct an object from settings if the object defines such
/// constructor, or default-construct the object otherwise.
template <typename Type>
std::enable_if_t<!std::is_constructible<Type, const RunSettings&>::value, Type> makeFromSettings(
    const RunSettings& UNUSED(settings)) {
    // default constructible
    return Type();
}

/// Specialization for types constructible from settings
template <typename Type>
std::enable_if_t<std::is_constructible<Type, const RunSettings&>::value, Type> makeFromSettings(
    const RunSettings& settings) {
    // constructible from settings
    return Type(settings);
}


/// \brief Implementation of the Balsara switch \cite Balsara_1995, designed to reduce artificial viscosity in
/// shear flows and avoid numerical issues, such as unphysical transport of angular momentum.

/// Balsara switch is a template, needs another artificial viscosity as a template parameter. The template
/// parameter shall be an \ref IEquationTerm; Balsara switch then forward all functions (initialize, finalize,
/// ...) to this base AV. Furthermore, the AV must define a class called Derivative with operator()(i, j),
/// preferably force inlined, returing value \f$\Pi_{ij}\f$ of the artificial viscosity between particles i
/// and j.
///
/// Using this term, Balsara switch decreases the artificial viscosity by factor:
/// \f[
///  f_{\rm Balsara} = \frac{| \nabla \cdot \vec v |}{|\nabla \cdot \vec v| + \|\nabla \times \vec v\| +
///  \epsilon c_{\rm s} / h} \,.
/// \f]
/// To conserve the total momentum, the term is symmetrized over particle pair, \f$f_{ij} = 0.5(f_i + f_j)\f$
template <typename AV>
class BalsaraSwitch : public IEquationTerm {

    static_assert(std::is_base_of<IEquationTerm, AV>::value, "AV must be derived from IEquationTerm");

    class Derivative : public AccelerationTemplate<Derivative> {
    private:
        ArrayView<const Float> cs;
        ArrayView<const Vector> r, v;
        ArrayView<const Float> divv;
        ArrayView<const Vector> rotv;
        typename AV::Derivative av;
        const Float eps = 1.e-4_f;

    public:
        explicit Derivative(const RunSettings& settings)
            : AccelerationTemplate<Derivative>(settings)
            , av(settings) {}

        INLINE void additionalCreate(Accumulated& results) {
            av.create(results);
        }

        INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
            ArrayView<const Vector> dummy;
            tie(r, v, dummy) = input.getAll<Vector>(QuantityId::POSITION);
            cs = input.getValue<Float>(QuantityId::SOUND_SPEED);
            divv = input.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
            rotv = input.getValue<Vector>(QuantityId::VELOCITY_ROTATION);

            av.initialize(input, results);
        }

        INLINE bool additionalEquals(const Derivative& other) const {
            return av.equals(other.av);
        }

        template <bool Symmetrize>
        INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& grad) {
            const Float Pi = 0.5_f * (factor(i) + factor(j)) * av.evalAv(i, j);
            SPH_ASSERT(isReal(Pi));
            const Float heating = 0.5_f * Pi * dot(v[i] - v[j], grad);
            return { Pi * grad, heating };
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
    explicit BalsaraSwitch(const RunSettings& settings)
        : av(makeFromSettings<AV>(settings)) {
        storeFactor = settings.get<bool>(RunSettingsId::SPH_AV_BALSARA_STORE);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        // no need to use the correction tensor here, velocity derivatives are only used to
        // compute the Balsara factor, which is an arbitrary correction to AV anyway
        derivatives.require(makeDerivative<VelocityDivergence>(settings));
        derivatives.require(makeDerivative<VelocityRotation>(settings));
        derivatives.require(makeAuto<Derivative>(settings));
    }

    virtual void initialize(IScheduler& scheduler, Storage& storage, const Float t) override {
        av.initialize(scheduler, storage, t);
    }

    virtual void finalize(IScheduler& scheduler, Storage& storage, const Float t) override {
        av.finalize(scheduler, storage, t);

        if (storeFactor) {
            ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
            ArrayView<const Vector> rotv = storage.getValue<Vector>(QuantityId::VELOCITY_ROTATION);
            ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
            ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
            ArrayView<Float> factor = storage.getValue<Float>(QuantityId::AV_BALSARA);
            for (Size i = 0; i < factor.size(); ++i) {
                const Float dv = abs(divv[i]);
                const Float rv = getLength(rotv[i]);
                factor[i] = dv / (dv + rv + 1.e-4_f * cs[i] / r[i][H]);
            }
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        storage.insert(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
        storage.insert(QuantityId::VELOCITY_ROTATION, OrderEnum::ZERO, Vector(0._f));
        if (storeFactor) {
            storage.insert(QuantityId::AV_BALSARA, OrderEnum::ZERO, 0._f);
        }
        av.create(storage, material);
    }
};

NAMESPACE_SPH_END
