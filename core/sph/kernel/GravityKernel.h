#pragma once

/// \file GravityKernel.h
/// \brief Smoothing kernels for including gravity into SPH
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2021

#include "common/Traits.h"
#include "math/Functional.h"
#include "objects/wrappers/Lut.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

/// \brief Gravity smoothing kernels associated with standard SPH kernels.
///
/// Needs to be specialized for every SPH kernel.
template <typename TKernel>
class GravityKernel;

struct GravityKernelTag {};

template <typename T, typename = void>
struct IsGravityKernel;

/// \brief Gravitational kernel approximated by LUT for close particles.
///
/// At larger distances, we recover the standard Newtonian inverse square law. Implemented according to
/// P. Cossins, PhD thesis, 2010 \cite Cossins_2010.
///
/// It can only be constructed from a precise gravitational kernel or using the default constructor, which
/// corresponds to a point mass. Gravitational kernels can be created by specializing \ref GravityKernel class
/// template or by deriving from GravityKernelTag. This is required to avoid accidentally creating \ref
/// GravityLutKernel from SPH kernel.
class GravityLutKernel {
private:
    /// Kernel for close particles
    LutKernel<3> close;

public:
    GravityLutKernel() = default;

    template <typename TKernel>
    GravityLutKernel(TKernel&& source)
        : close(std::forward<TKernel>(source)) {
        using T = std::decay_t<TKernel>;
        static_assert(IsGravityKernel<T>::value,
            "Use GravityKernel to get gravity smoothing kernel associated to SPH kernel");
    }

    INLINE Float radius() const {
        return close.radius();
    }

    INLINE Float value(const Vector& r, const Float h) const {
        SPH_ASSERT(h >= 0._f);
        const Float hInv = 1._f / h;
        const Float qSqr = getSqrLength(r * hInv);
        if (qSqr + EPS >= sqr(close.radius())) {
            return -1._f / getLength(r);
        } else {
            // LUT kernel returns 0 for qSqr >= radiusSqr
            const Float value = close.valueImpl(qSqr);
            SPH_ASSERT(value < 0._f);
            return hInv * value;
        }
    }

    INLINE Vector grad(const Vector& r, const Float h) const {
        SPH_ASSERT(h >= 0._f);
        const Float hInv = 1._f / h;
        const Float qSqr = getSqrLength(r * hInv);
        if (qSqr + EPS >= sqr(close.radius())) {
            return r / pow<3>(getLength(r));
        } else {
            const Float grad = close.gradImpl(qSqr);
            SPH_ASSERT(grad != 0._f);
            return pow<3>(hInv) * r * grad;
        }
    }
};

template <>
class GravityKernel<CubicSpline<3>> : public Kernel<GravityKernel<CubicSpline<3>>, 3> {
public:
    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        SPH_ASSERT(qSqr >= 0._f && qSqr <= sqr(radius()));
        const Float q = sqrt(qSqr);
        if (q < 1._f) {
            return 2._f / 3._f * qSqr - 3._f / 10._f * pow<4>(q) + 1._f / 10._f * pow<5>(q) - 7._f / 5._f;
        } else {
            return 4._f / 3._f * qSqr - pow<3>(q) + 3._f / 10._f * pow<2>(qSqr) - 1._f / 30._f * pow<5>(q) -
                   8._f / 5._f + 1._f / (15._f * q);
        }
    }

    INLINE Float gradImpl(const Float qSqr) const {
        SPH_ASSERT(qSqr >= 0._f && qSqr <= sqr(radius()));
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return 4._f / 3._f;
        } else if (q < 1._f) {
            return 1._f / q * (4._f / 3._f * q - 6._f / 5._f * pow<3>(q) + 1._f / 2._f * pow<2>(qSqr));
        } else {
            return 1._f / q *
                   (8._f / 3._f * q - 3._f * qSqr + 6._f / 5._f * pow<3>(q) - 1._f / 6._f * pow<2>(qSqr) -
                       1._f / (15._f * qSqr));
        }
    }
};

/// ThomasCouchmanKernel differs from CubicSpline only in the gradient, so the GravityKernel is the same.
template <>
class GravityKernel<ThomasCouchmanKernel<3>> : public GravityKernel<CubicSpline<3>> {};

/// Gravity kernel of a solid sphere
class SolidSphereKernel : public Kernel<SolidSphereKernel, 3>, public GravityKernelTag {
public:
    SolidSphereKernel() = default;

    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float UNUSED(qSqr)) const {
        return 0._f; /// \todo
    }

    INLINE Float gradImpl(const Float UNUSED(qSqr)) const {
        return 1._f;
    }
};

/// \brief Computes the gravitational softening kernel from the associated SPH kernel by integrating the
/// Poisson equation.
template <typename TKernel,
    typename = std::enable_if_t<IsKernel<TKernel>::value && !IsGravityKernel<TKernel>::value>>
auto getAssociatedGravityKernel(const TKernel& W, const Size resolution = 40000) {
    Array<Float> psi(resolution);
    const Float radius = W.radius();
    Float integral = 0._f;
    psi[0] = 0._f;
    Float q1 = 0._f;
    for (Size i = 1; i < resolution; ++i) {
        const Float q2 = Float(i) / resolution * radius;
        integral += integrate(Interval(q1, q2), [&W](const Float q) { return sqr(q) * W.valueImpl(sqr(q)); });
        psi[i] = 4._f * PI / sqr(q2) * integral;
        SPH_ASSERT(isReal(psi[i]));

        q1 = q2;
    }

    class KernelImpl : public Kernel<KernelImpl, 3>, public GravityKernelTag {
        Lut<Float> values;
        Lut<Float> gradients;
        Float grad0;

    public:
        KernelImpl(Array<Float>&& psi, const Float grad0, const Interval& range)
            : gradients(range, std::move(psi))
            , grad0(grad0) {
            const Float r = range.upper();
            values = gradients.integral(r, -1._f / r);
        }

        INLINE Float valueImpl(const Float qSqr) const {
            return values(sqrt(qSqr));
        }
        INLINE Float gradImpl(const Float qSqr) const {
            const Float q = sqrt(qSqr);
            if (q == 0._f) {
                return grad0;
            }
            return gradients(q) / q;
        }
        INLINE Float radius() const {
            return values.getRange().upper();
        }
    };

    // integral q^2 dq = 1/3 * q^3
    const Float grad0 = 4._f / 3._f * PI * W.valueImpl(0._f);
    return KernelImpl(std::move(psi), grad0, Interval(0._f, radius));
}

template <typename T, typename>
struct IsGravityKernel {
    static constexpr bool value = false;
};

template <typename T>
struct IsGravityKernel<GravityKernel<T>> {
    static constexpr bool value = IsKernel<T>::value;
};

template <typename T>
struct IsGravityKernel<T, std::enable_if_t<std::is_base_of<GravityKernelTag, std::decay_t<T>>::value>> {
    static constexpr bool value = IsKernel<T>::value;
};

NAMESPACE_SPH_END
