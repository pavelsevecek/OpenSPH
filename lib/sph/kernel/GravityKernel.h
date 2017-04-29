#pragma once

/// Smoothing kernels for including gravity into SPH
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// Kernel is approximated by LUT for close particles, at larger distances we recover the standard Newtonian
/// inverse square law.
/// According to Peter Cossins, PhD thesis, 2010

class GravityLutKernel {
private:
    /// Kernel for close particles
    LutKernel<3> close;

public:
    template <typename TKernel>
    GravityLutKernel(TKernel&& source)
        : close(std::forward<TKernel>(source)) {}

    INLINE float closeRadius() const {
        return close.radius();
    }

    INLINE Float value(const Vector& r, const Float h) const {
        ASSERT(h > EPS);
        const Float hInv = 1._f / h;
        const Float qSqr = getSqrLength(r * hInv);
        if (qSqr > sqr(close.radius())) {
            return -1._f / getLength(r);
        } else {
            return hInv * close.valueImpl(qSqr);
        }
    }

    INLINE Vector grad(const Vector& r, const Float h) const {
        ASSERT(h > EPS);
        const Float hSqrInv = 1._f / sqr(h);
        const Float qSqr = getSqrLength(r) * hSqrInv;
        if (qSqr > sqr(close.radius())) {
            return r / pow<3>(getLength(r));
        } else {
            return hSqrInv * r * close.gradImpl(qSqr);
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
        ASSERT(qSqr > 0._f && qSqr <= sqr(radius()));
        const Float q = sqrt(qSqr);
        if (q < 1._f) {
            return 2._f / 3._f * qSqr - 3._f / 10._f * pow<3>(q) + 1._f / 10._f * pow<5>(q) - 7._f / 5._f;
        } else {
            return 4._f / 3._f * qSqr - pow<3>(q) + 3._f / 10._f * pow<2>(qSqr) - 1._f / 30._f * pow<5>(q) -
                   8._f / 5._f + 1._f / (15._f * q);
        }
    }

    INLINE Float gradImpl(const Float qSqr) const {
        ASSERT(qSqr > 0._f && qSqr <= sqr(radius()));
        const Float q = sqrt(qSqr);
        if (q < 1._f) {
            return 4._f / 3._f * q + 6._f / 5._f * pow<3>(q) + 1._f / 2._f * pow<2>(qSqr);
        } else {
            return 8._f / 3._f * q - 3._f * qSqr + 6._f / 5._f * pow<3>(q) - 1._f / 6._f * pow<2>(qSqr) -
                   1._f / (15._f * qSqr);
        }
    }
};


NAMESPACE_SPH_END
