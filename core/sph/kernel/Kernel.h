#pragma once

/// \file Kernel.h
/// \brief SPH kernels
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// \brief Base class for all SPH kernels.
///
/// Provides an interface for computing kernel values and gradients. All derived class must implement method
/// <code>valueImpl</code> and <code>gradImpl</code>. Both function take SQUARED value of dimensionless
/// distance q as a parameter. Function value returns the kernel value, grad returns gradient DIVIDED BY q.
template <typename TDerived, Size D>
class Kernel : public Noncopyable {
public:
    Kernel() = default;

    /// Value of kernel at given point
    /// this should be called only once for a pair of particles as there is expensive division
    /// \todo Potentially precompute the 3rd power ...
    INLINE Float value(const Vector& r, const Float h) const noexcept {
        SPH_ASSERT(h > 0._f);
        const Float hInv = 1._f / h;
        return pow<D>(hInv) * impl().valueImpl(getSqrLength(r) * sqr(hInv));
    }

    INLINE Vector grad(const Vector& r, const Float h) const noexcept {
        SPH_ASSERT(h > 0._f);
        const Float hInv = 1._f / h;
        return r * pow<D + 2>(hInv) * impl().gradImpl(getSqrLength(r) * sqr(hInv));
    }

private:
    INLINE const TDerived& impl() const noexcept {
        return static_cast<const TDerived&>(*this);
    }
};


/// \brief A look-up table approximation of the kernel.
///
/// Can be constructed from any SPH kernel. Use this class exclusively for any high-performance computations,
/// it is always faster than using kernel functions directly (except for trivial kerneles, such as \ref
/// TriangleKernel). The precision difference is about 1.e-6.
template <Size D>
class LutKernel : public Kernel<LutKernel<D>, D> {
private:
    static constexpr Size NEntries = 40000;

    Array<Float> values;
    Array<Float> grads;

    Float rad = 0._f;
    Float qSqrToIdx = 0._f;

public:
    LutKernel() = default;

    LutKernel(const LutKernel& other) {
        *this = other;
    }

    LutKernel(LutKernel&& other) {
        *this = std::move(other);
    }

    LutKernel& operator=(const LutKernel& other) {
        rad = other.rad;
        qSqrToIdx = other.qSqrToIdx;
        values = copyable(other.values);
        grads = copyable(other.grads);
        return *this;
    }

    LutKernel& operator=(LutKernel&& other) = default;

    /// \brief Constructs LUT kernel given an exact SPH kernel.
    template <typename TKernel,
        typename = std::enable_if_t<!std::is_same<std::decay_t<TKernel>, LutKernel<D>>::value>>
    LutKernel(TKernel&& source) {
        rad = source.radius();

        SPH_ASSERT(rad > 0._f);
        const Float radInvSqr = 1._f / (rad * rad);
        qSqrToIdx = Float(NEntries) * radInvSqr;

        // allocate and set NEntries + 1 for correct interpolation of the last value
        values.resize(NEntries + 1);
        grads.resize(NEntries + 1);
        for (Size i = 0; i < NEntries + 1; ++i) {
            const Float qSqr = Float(i) / qSqrToIdx;
            values[i] = source.valueImpl(qSqr);
            grads[i] = source.gradImpl(qSqr);
        }
        /// \todo re-normalize?
    }

    INLINE bool isInit() const {
        return rad > 0._f;
    }

    INLINE Float radius() const noexcept {
        return rad;
    }

    INLINE Float valueImpl(const Float qSqr) const noexcept {
        SPH_ASSERT(qSqr >= 0._f);
        SPH_ASSERT(isInit());
        if (SPH_UNLIKELY(qSqr >= sqr(rad))) {
            // outside of kernel support
            return 0._f;
        }
        // linear interpolation of stored values
        const Float floatIdx = qSqrToIdx * qSqr;
        const Size idx1 = Size(floatIdx);
        SPH_ASSERT(idx1 < NEntries);
        const Size idx2 = idx1 + 1;
        const Float ratio = floatIdx - Float(idx1);
        SPH_ASSERT(ratio >= 0._f && ratio < 1._f);

        return values[idx1] * (1._f - ratio) + values[idx2] * ratio;
    }

    INLINE Float gradImpl(const Float qSqr) const noexcept {
        SPH_ASSERT(qSqr >= 0._f);
        SPH_ASSERT(isInit());
        if (SPH_UNLIKELY(qSqr >= sqr(rad))) {
            // outside of kernel support
            return 0._f;
        }
        const Float floatIdx = qSqrToIdx * qSqr;
        const Size idx1 = Size(floatIdx);
        SPH_ASSERT(unsigned(idx1) < unsigned(NEntries));
        const Size idx2 = idx1 + 1;
        const Float ratio = floatIdx - Float(idx1);
        SPH_ASSERT(ratio >= 0._f && ratio < 1._f);

        return grads[idx1] * (1._f - ratio) + grads[idx2] * ratio;
    }
};


/// \brief Cubic spline (M4) kernel
template <Size D>
class CubicSpline : public Kernel<CubicSpline<D>, D> {
private:
    const Float normalization[3] = { 2._f / 3._f, 10._f / (7._f * PI), 1._f / PI };

public:
    INLINE Float radius() const {
        return 2._f;
    }

    // template <bool TApprox = false>
    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 1._f) {
            return normalization[D - 1] * (0.25_f * pow<3>(2._f - q) - pow<3>(1._f - q));
        }
        if (q < 2._f) {
            return normalization[D - 1] * (0.25_f * pow<3>(2._f - q));
        }
        return 0._f; // compact within 2r radius
    }

    // template <bool TApprox = false>
    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            // gradient of kernel is 0 at q = 0, but here we divide by q,
            // the expression grad/q has a finite limit for q->0
            return -3._f * normalization[D - 1];
        }
        if (q < 1._f) {
            return (1._f / q) * normalization[D - 1] * (-0.75_f * pow<2>(2._f - q) + 3._f * pow<2>(1._f - q));
        }
        if (q < 2._f) {
            return (1._f / q) * normalization[D - 1] * (-0.75_f * pow<2>(2._f - q));
        }
        return 0._f;
    }
};

/// \brief Fourth-order spline (M5) kernel
template <Size D>
class FourthOrderSpline : public Kernel<FourthOrderSpline<D>, D> {
private:
    const Float normalization[3] = { 1._f / 24._f, 96._f / (1199._f * PI), 1._f / (20._f * PI) };

public:
    INLINE Float radius() const {
        return 2.5_f;
    }

    // template <bool TApprox = false>
    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 0.5_f) {
            return normalization[D - 1] *
                   (pow<4>(2.5_f - q) - 5._f * pow<4>(1.5_f - q) + 10._f * pow<4>(0.5_f - q));
        }
        if (q < 1.5_f) {
            return normalization[D - 1] * (pow<4>(2.5_f - q) - 5._f * pow<4>(1.5_f - q));
        }
        if (q < 2.5_f) {
            return normalization[D - 1] * (pow<4>(2.5_f - q));
        }
        return 0._f; // compact within 2r radius
    }

    // template <bool TApprox = false>
    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -30._f * normalization[D - 1];
        }
        if (q < 0.5_f) {
            return (1._f / q) * normalization[D - 1] *
                   (-4._f * pow<3>(2.5_f - q) + 20._f * pow<3>(1.5_f - q) - 40._f * pow<3>(0.5_f - q));
        }
        if (q < 1.5_f) {
            return (1._f / q) * normalization[D - 1] *
                   (-4._f * pow<3>(2.5_f - q) + 20._f * pow<3>(1.5_f - q));
        }
        if (q < 2.5_f) {
            return (1._f / q) * normalization[D - 1] * (-4._f * pow<3>(2.5_f - q));
        }
        return 0._f;
    }
};

/// \brief Kernel proposed by Read et al. (2010) with improved stability.
///
/// Defined only for 3 dimensions.
class CoreTriangle : public Kernel<CoreTriangle, 3> {
private:
    const Float alpha = 1._f / 3._f;
    const Float beta = 1._f + 6._f * sqr(alpha) - 12._f * pow<3>(alpha);
    const Float normalization = 8._f / (PI * (6.4_f * pow<5>(alpha) - 16._f * pow<6>(alpha) + 1._f));

public:
    INLINE Float radius() const {
        return 1._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);

        if (q < alpha) {
            return normalization * ((-12._f * alpha + 18._f * sqr(alpha)) * q + beta);
        } else if (q < 0.5_f) {
            return normalization * (1._f - 6._f * sqr(q) * (1._f - q));
        } else if (q < 1._f) {
            return normalization * 2._f * pow<3>(1._f - q);
        } else {
            return 0._f;
        }
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -100._f;
        } else if (q < alpha) {
            return normalization / q * (-12._f * alpha + 18._f * sqr(alpha));
        } else if (q < 0.5_f) {
            return normalization / q * (-12._f * q + 18._f * sqr(q));
        } else if (q < 1._f) {
            return normalization / q * (-6._f * sqr(1._f - q));
        } else {
            return 0._f;
        }
    }
};

/// \brief Kernel introduced by Thomas & Couchman (1992).
///
/// The kernel values are the same as for cubic spline, but the gradient is modified, adding a small repulsive
/// force. This attempts to prevent particle clustering.
template <Size D>
class ThomasCouchmanKernel : public Kernel<ThomasCouchmanKernel<D>, D> {
private:
    CubicSpline<D> M4;

    const Float normalization[3] = { 2._f / 3._f, 10._f / (7._f * PI), 1._f / PI };

public:
    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        return M4.valueImpl(qSqr);
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            // this kernel has discontinuous gradient - it is nonzero for q->0, so the value for q = 0 is
            // undefined (it is a "0/0" expression). To avoid this, return a reasonably high (nonzero) number.
            return -100._f;
        }
        if (q < 2._f / 3._f) {
            return -(1._f / q) * normalization[D - 1];
        }
        if (q < 1._f) {
            return (1._f / q) * normalization[D - 1] * (-0.75_f * q * (4._f - 3._f * q));
        }
        if (q < 2._f) {
            return (1._f / q) * normalization[D - 1] * (-0.75_f * pow<2>(2._f - q));
        }
        return 0._f;
    }
};

class WendlandC2 : public Kernel<WendlandC2, 3> {
private:
    const Float normalization = 21._f / (16._f * PI);

public:
    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 2._f) {
            return normalization * pow<4>(1._f - 0.5_f * q) * (2._f * q + 1._f);
        }
        return 0._f;
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -5._f * normalization;
        }
        if (q < 2._f) {
            return (1._f / q) * normalization * 0.625_f * pow<3>(q - 2._f) * q;
        }
        return 0._f;
    }
};

class WendlandC4 : public Kernel<WendlandC4, 3> {
private:
    const Float normalization = 495._f / (256._f * PI);

public:
    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 2._f) {
            return normalization * pow<6>(1._f - 0.5_f * q) * (35._f / 12._f * pow<2>(q) + 3._f * q + 1._f);
        }
        return 0._f;
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -14._f / 3._f * normalization;
        }
        if (q < 2._f) {
            return (1._f / q) * normalization *
                   (7._f / 96._f * q *
                       (5._f * pow<6>(q) - 48._f * pow<5>(q) + 180._f * pow<4>(q) - 320._f * pow<3>(q) +
                           240._f * pow<2>(q) - 64._f));
        }
        return 0._f;
    }
};

class WendlandC6 : public Kernel<WendlandC6, 3> {
private:
    const Float normalization = 1365._f / (512._f * PI);

public:
    INLINE Float radius() const {
        return 2._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 2._f) {
            return normalization * pow<8>(1._f - 0.5_f * q) *
                   (4._f * pow<3>(q) + 25._f / 4._f * pow<2>(q) + 4._f * q + 1._f);
        }
        return 0._f;
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -5.5_f * normalization;
        }
        if (q < 2._f) {
            return (1._f / q) * normalization * 0.0214844_f * pow<7>(q - 2._f) * q *
                   (8._f * pow<2>(q) + 7._f * q + 2._f);
        }
        return 0._f;
    }
};

/// Poly-6 kernel of Muller et al. 2003
class Poly6 : public Kernel<Poly6, 3> {
private:
    const Float normalization = 315._f / (64._f * PI);

public:
    INLINE Float radius() const {
        return 1._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 1._f) {
            return normalization * pow<3>(1._f - pow<2>(q));
        }
        return 0._f;
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -6._f * normalization;
        } else if (q < 1._f) {
            return (1._f / q) * normalization * 3 * pow<2>(1._f - pow<2>(q)) * (-2 * q);
        }
        return 0._f;
    }
};

/// Spiky kernel of Muller et al. 2003
class SpikyKernel : public Kernel<SpikyKernel, 3> {
private:
    const Float normalization = 15._f / PI;

public:
    INLINE Float radius() const {
        return 1._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        SPH_ASSERT(q >= 0);
        if (q < 1._f) {
            return normalization * pow<3>(1._f - q);
        }
        return 0._f;
    }

    INLINE Float gradImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        if (q == 0._f) {
            return -100._f;
        } else if (q < 1._f) {
            return (1._f / q) * normalization * 3 * pow<2>(1._f - q) * (-1);
        }
        return 0._f;
    }
};

/// \brief Gaussian kernel
///
/// Clamped to zero at radius 5, the error is therefore about exp(-5^2) = 10^-11.
template <Size D>
class Gaussian : public Kernel<Gaussian<D>, D> {
private:
    const Float normalization[3] = { 1._f / sqrt(PI), 1._f / PI, 1._f / (PI * sqrt(PI)) };

public:
    INLINE Float radius() const {
        return 5._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        if (qSqr >= sqr(radius())) {
            return 0._f;
        }
        return normalization[D - 1] * exp(-qSqr);
    }

    INLINE Float gradImpl(const Float qSqr) const {
        if (qSqr >= sqr(radius())) {
            return 0._f;
        }
        if (qSqr == 0._f) {
            return -2._f * normalization[D - 1];
        }
        const Float q = sqrt(qSqr);
        return normalization[D - 1] / q * exp(-qSqr) * (-2._f * q);
    }
};

/// \brief Triangular (piecewise linear) kernel.
///
/// Does not have continuous derivatives, mainly for testing purposes and non-SPH applications.
template <Size D>
class TriangleKernel : public Kernel<TriangleKernel<D>, D> {
private:
    const Float normalization[3] = { 1._f, 3._f / PI, 3._f / PI };

public:
    INLINE Float radius() const {
        return 1._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        if (qSqr >= sqr(radius())) {
            return 0._f;
        }
        const Float q = sqrt(qSqr);
        return normalization[D - 1] * (1._f - q);
    }

    INLINE Float gradImpl(const Float qSqr) const {
        if (qSqr >= sqr(radius())) {
            return 0._f;
        }
        // unfortunately this gradient is nonzero at q->0, so grad/q diverges;
        // let's return a reasonable value to avoid numerical problems
        if (qSqr == 0._f) {
            return -100._f;
        }
        const Float q = sqrt(qSqr);
        return -normalization[D - 1] / q;
    }
};

/// \brief Helper kernel wrapper that modifies the support of another kernel.
template <Size D, typename TKernel>
class ScalingKernel : public Kernel<ScalingKernel<D, TKernel>, D> {
private:
    TKernel kernel;
    Float scaling;

public:
    explicit ScalingKernel(const Float newRadius) {
        scaling = newRadius / kernel.radius();
    }

    INLINE Float radius() const {
        return scaling * kernel.radius();
    }

    INLINE Float valueImpl(const Float qSqr) const {
        return kernel.valueImpl(qSqr / sqr(scaling)) / pow<D>(scaling);
    }

    INLINE Float gradImpl(const Float qSqr) const {
        return kernel.gradImpl(qSqr / sqr(scaling)) / pow<D + 2>(scaling);
    }
};

/// \brief SPH approximation of laplacian, computed from a kernel gradient.
///
/// Is more stable than directly applying second derivatives to kernel and has the same error O(h^2). Can be
/// used to compute laplacian of both scalar and vector quantities.
/// \param value Scalar or vector value from which we compute the laplacian
/// \param grad Kernel gradient corresponding to vector dr
///
/// \note Note that the sign is different compared to the Eq. (95) of \cite Price_2010. This is correct,
/// provided the value is computed as v[j]-v[i], dr is computed as r[j]-r[i] and grad is computed as grad
/// W(r[j]-r[i]).
template <typename T>
INLINE T laplacian(const T& value, const Vector& grad, const Vector& dr) {
    SPH_ASSERT(dr != Vector(0._f));
    return 2._f * value * dot(dr, grad) / getSqrLength(dr);
}

/// \brief Second derivative of vector quantity, applying gradient on a divergence.
///
/// Doesn't make sense for scalar quantities. See Price 2010 \cite Price_2010
INLINE Vector gradientOfDivergence(const Vector& value, const Vector& grad, const Vector& dr) {
    SPH_ASSERT(dr != Vector(0._f));
    const Float rSqr = getSqrLength(dr);
    const Float f = dot(dr, grad) / rSqr;
    return (DIMENSIONS + 2) * dot(value, dr) * dr * f / rSqr - value * f;
}


/// Symmetrization of the kernel with a respect to different smoothing lenths
/// Two possibilities - Symmetrized kernel W_ij = 0.5(W_i + W_j)
///                   - Symmetrized smoothing length h_ij = 0.5(h_i + h_j)
template <typename TKernel>
class SymmetrizeValues {
private:
    TKernel kernel;

public:
    template <typename... TArgs>
    SymmetrizeValues(TArgs&&... args)
        : kernel(std::forward<TArgs>(args)...) {}

    INLINE Float value(const Vector& r1, const Vector& r2) const {
        SPH_ASSERT(r1[H] > 0 && r2[H] > 0, r1[H], r2[H]);
        return 0.5_f * (kernel.value(r1 - r2, r1[H]) + kernel.value(r1 - r2, r2[H]));
    }

    INLINE Vector grad(const Vector& r1, const Vector& r2) const {
        SPH_ASSERT(r1[H] > 0 && r2[H] > 0, r1[H], r2[H]);
        return 0.5_f * (kernel.grad(r1 - r2, r1[H]) + kernel.grad(r1 - r2, r2[H]));
    }

    INLINE Float radius() const {
        return kernel.radius();
    }
};

template <typename TKernel>
class SymmetrizeSmoothingLengths {
private:
    TKernel kernel;

public:
    template <typename... TArgs>
    SymmetrizeSmoothingLengths(TArgs&&... args)
        : kernel(std::forward<TArgs>(args)...) {}

    INLINE Float value(const Vector& r1, const Vector& r2) const {
        SPH_ASSERT(r1[H] > 0 && r2[H] > 0, r1[H], r2[H]);
        return kernel.value(r1 - r2, 0.5_f * (r1[H] + r2[H]));
    }

    INLINE Vector grad(const Vector& r1, const Vector& r2) const {
        SPH_ASSERT(r1[H] > 0 && r2[H] > 0, r1[H], r2[H]);
        return kernel.grad(r1 - r2, 0.5_f * (r1[H] + r2[H]));
    }

    INLINE Float radius() const {
        return kernel.radius();
    }
};

template <typename T, typename = void>
struct IsKernel {
    static constexpr bool value = false;
};

template <typename T>
struct IsKernel<T, VoidType<decltype(std::declval<T>().radius())>> {
    static constexpr bool value = true;
};

NAMESPACE_SPH_END
