#pragma once

/// Implementation of the Balsara switch (Balsara, 1995), designed to reduce artificial viscosity in shear
/// flows. Needs another artificial viscosity as a base object, Balsara switch is just a modifier.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/EquationTerm.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/*class BalsaraSwitch : public Abstract::EquationTerm {
private:
    class Term : public Abstract::Derivative {
    private:
        ArrayView<Float> cs;
        ArrayView<Vector> r;

    public:
        virtual void initialize(const Storage& input, Accumulated& output) {
            cs = input.getValue <
        }
    };
    // accumulator, compute new values of div v and rot v in each time step
    Rotv accumulatedRotV;
    Divv accumulatedDivV; /// \todo divv can be possibly shared with Morris & Monaghan AV

    // values of div v and rot v computed previous time step
    ArrayView<Float> divv;
    ArrayView<Vector> rotv;
    const Float eps = 1.e-4_f;

public:
    template <typename... TArgs>
    BalsaraSwitch(TArgs&&... args)
        : Module<AV, Rotv, Divv>(av, accumulatedRotV, accumulatedDivV)
        , av(std::forward<TArgs>(args)...)
        , accumulatedRotV(QuantityId::VELOCITY_ROTATION)
        , accumulatedDivV(QuantityId::VELOCITY_DIVERGENCE) {}

    void initialize(Storage& storage, const BodySettings& settings) {
        /// \todo set initial values of rot v and div v
        storage.insert<Vector, OrderEnum::ZERO>(QuantityId::VELOCITY_ROTATION, Vector(0._f));
        storage.insert<Float, OrderEnum::ZERO>(QuantityId::VELOCITY_DIVERGENCE, 0._f);
        this->initializeModules(storage, settings);
    }

    void update(Storage& storage) {
        tie(cs, divv) = storage.getValues<Float>(QuantityId::SOUND_SPEED, QuantityId::VELOCITY_DIVERGENCE);
        tie(r, rotv) = storage.getValues<Vector>(QuantityId::POSITIONS, QuantityId::VELOCITY_ROTATION);
        this->updateModules(storage);
    }

    INLINE void accumulate(const Size i, const Size j, const Vector& grad) {
        this->accumulateModules(i, j, grad);
    }

    INLINE void integrate(Storage& storage) {
        this->integrateModules(storage);
    }

    /// Returns the artificial viscosity Pi_ij after applying Balsara switch. Needs to be multiplied by
    /// kernel
    /// gradient to get the final force due to AV.
    INLINE Float operator()(const Size i, const Size j) {
        return 0.5_f * (getFactor(i) + getFactor(j)) * av(i, j);
    }

    /// Returns the Balsara factor for i-th particle. Mainly for testing purposes, operator() applies
    /// Balsara
    /// switch with no need to explicitly call getFactor by the user.
    INLINE Float getFactor(const Size i) {
        const Float dv = abs(divv[i]);
        const Float rv = getLength(rotv[i]);
        return dv / (dv + rv + eps * cs[i] / r[i][H]);
    }
};*/


NAMESPACE_SPH_END
