#pragma once

/// \file GravityKernel.h
/// \brief Smoothing kernels for including gravity into SPH
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2017

#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// Kernel is approximated by LUT for close particles, at larger distances we recover the standard Newtonian
/// inverse square law.
/// According to P. Cossins, PhD thesis, 2010 \cite Cossins_2010
class GravityLutKernel {
private:
    /// Kernel for close particles
    LutKernel<3> close;

public:
    GravityLutKernel() = default;

    template <typename TKernel>
    GravityLutKernel(TKernel&& source)
        : close(std::forward<TKernel>(source)) {}

    INLINE float closeRadius() const {
        return close.radius();
    }

    INLINE Float value(const Vector& r, const Float h) const {
        ASSERT(h >= 0._f);
        const Float hInv = 1._f / h;
        const Float qSqr = getSqrLength(r * hInv);
        if (qSqr + EPS >= sqr(close.radius())) {
            return -1._f / getLength(r);
        } else {
            // LUT kernel returns 0 for qSqr >= radiusSqr
            const Float value = close.valueImpl(qSqr);
            ASSERT(value < 0._f);
            return hInv * value;
        }
    }

    INLINE Vector grad(const Vector& r, const Float h) const {
        ASSERT(h >= 0._f);
        const Float hInv = 1._f / h;
        const Float qSqr = getSqrLength(r * hInv);
        if (qSqr + EPS >= sqr(close.radius())) {
            return r / pow<3>(getLength(r));
        } else {
            const Float grad = close.gradImpl(qSqr);
            ASSERT(grad != 0._f);
            return pow<3>(hInv) * r * grad;
        }
    }
};

/// Gravity smoothing kernels associated with standard SPH kernels
template <typename TKernel>
class GravityKernel;

template <>
class GravityKernel<CubicSpline<3>> : public Kernel<GravityKernel<CubicSpline<3>>, 3> {
public:
    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        ASSERT(qSqr >= 0._f && qSqr <= sqr(radius()));
        const Float q = sqrt(qSqr);
        if (q < 1._f) {
            return 2._f / 3._f * qSqr - 3._f / 10._f * pow<4>(q) + 1._f / 10._f * pow<5>(q) - 7._f / 5._f;
        } else {
            return 4._f / 3._f * qSqr - pow<3>(q) + 3._f / 10._f * pow<2>(qSqr) - 1._f / 30._f * pow<5>(q) -
                   8._f / 5._f + 1._f / (15._f * q);
        }
    }

    INLINE Float gradImpl(const Float qSqr) const {
        ASSERT(qSqr >= 0._f && qSqr <= sqr(radius()));
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return 4._f / 3._f;
        } else if (q < 1._f) {
            return 1._f / q * (4._f / 3._f * q - 6._f / 5._f * pow<3>(q) + 1._f / 2._f * pow<2>(qSqr));
        } else {
            return 1._f / q * (8._f / 3._f * q - 3._f * qSqr + 6._f / 5._f * pow<3>(q) -
                                  1._f / 6._f * pow<2>(qSqr) - 1._f / (15._f * qSqr));
        }
    }
};


NAMESPACE_SPH_END
