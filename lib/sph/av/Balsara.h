#pragma once

/// Implementation of the Balsara switch (Balsara, 1995), designed to reduce artificial viscosity in shear
/// flows. Needs another artificial viscosity as a base object, Balsara switch is just a modifier.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "solvers/Accumulator.h"
#include "solvers/Module.h"
#include "storage/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

template <typename AV>
class BalsaraSwitch : public Module<AV, Rotv, Divv> {
private:
    ArrayView<Float> cs;
    ArrayView<Vector> r;
    AV av;
    Rotv rotv;
    Divv divv;

    const Float eps = 1.e-4_f;

public:
    template <typename... TArgs>
    BalsaraSwitch(TArgs&&... args)
        : Module<AV, Rotv, Divv>(av, rotv, divv)
        , av(std::forward<TArgs>(args)...) {}

    void update(Storage& storage) {
        cs = storage.getValue<Float>(QuantityKey::CS);
        r = storage.getValue<Vector>(QuantityKey::R);
        this->updateModules(storage);
    }

    /// \todo implement swapping of accumulated values with stored quantities
    INLINE Float operator()(const int i, const int j) { return 0.5_f * (get(i) + get(j)) * av(i, j); }

    INLINE void accumulate(const int i, const int j, const Vector& grad) {
        this->accumulateModules(i, j, grad);
    }

    INLINE void integrate(Storage& UNUSED(storage)) {}

private:
    INLINE Float get(const int i) {
        const Float dv = Math::abs(divv[i]);
        const Float rv = getLength(rotv[i]);
        return dv / (dv + rv + eps * cs[i] / r[i][H]);
    }
};


NAMESPACE_SPH_END
