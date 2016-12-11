#pragma once

/// Implementation of the Balsara switch (Balsara, 1995), designed to reduce artificial viscosity in shear
/// flows. Needs another artificial viscosity as a base object, Balsara switch is just a modifier.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

/// 4 types of quantitites
/// 1) constant - does not change at all
/// 2) derived - quantitites computed from othe rquantitites (using EOS, for example)
/// 3) evolved using evolutionary equation (done in timestepping)
/// 4) computed by direct summation (density in summation solver, energy in density independent solver, divv, rotv, ...)
///
/// We don't have to care about constant quantities.
/// Derived quantities - provide a functor to compute them
/// Evolved quantities - provide a functor which computes derivative increment
/// Summation quantities - provide a functor which compute quantity increment
///
/// Components of solver:
/// AV
/// fragmentation
///


struct Rotv {
    Vector operator()(const int i, const Float kernel, const Vector grad) {
        return cross(v[i], grad);
    }
};

struct Density {
    Float operator()(const int i, const Float kernel, const Vector grad) {
        return m[i] * kernel;
    }
};

struct Mass {

};

template <typename AV>
class BalsaraSwitch : public Object {
private:
    Array<Float>& cs;
    Array<Vector>& v;
    Array<Vector>& r;
    Array<Float>& divv;
    Array<Vector>& rotv;
    AV av;
    const Float eps = 1.e-4_f;

public:
    template <typename... TArgs>
    BalsaraSwitch(TArgs&&... args)
        : av(std::forward<TArgs>(args)...) {}

    INLINE Float operator()(const int i, const int j) { return 0.5_f * (get(i) + get(j)) * av(i, j); }

private:
    INLINE Float get(const int i) {
        const Float dv = Math::abs(divv[i]);
        const Float rv = getLength(rotv[i]);
        return divv / (divv + rotv + eps * cs[i] / r[i][H]);
    }
};


NAMESPACE_SPH_END
