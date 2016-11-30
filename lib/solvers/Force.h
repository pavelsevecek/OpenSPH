#pragma once


#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN

/// Forces acting on SPH particles.

/// not only force, also energy, these have to consistent at all times
template <typename TArtificialViscosity>
struct PressureForce {
    TArtificialViscosity av;

    INLINE Float operator()(const int i, const int j, const Vector grad) {
        /// \todo generalize for tensor AV
        return (p[i] / Math::sqr(rho[i]) + p[j] / Math::sqr(rho[j]) + av(i, j)) * grad;
    }

    INLINE Float energy(const int i, const int j, const Vector grad) {
        const Float divv = dot(v[i] - v[j], grad);
        return m[j] * divv * (p[i] / Math::sqr(rho[i])  + 0.5_f * av(i, j));
    }
};

template <typename TArtificialViscosity>
struct StressForce {
    TArtificialViscosity av;
    PressureForce<TArficialViscosity> presureTerm;

    INLINE Float operator()(const Vector grad) {
        // artificial viscosity is already accounted for in the pressure term
        return pressureTerm(grad) + (s[i] / Math::sqr(rho[i]) + s[j] / Math::sqr(rho[j])).apply(grad);
    }

    INLINE Float energy(const Vector grad) {
        return pressureTerm(i, j, grad) + 1._f / rho[i] * ddot(s[i], edot[i]);
    }
};

NAMESPACE_SPH_END
