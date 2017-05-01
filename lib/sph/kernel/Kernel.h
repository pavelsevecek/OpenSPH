#pragma once

/// \file Kernel.h
/// \brief SPH kernels
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2017

#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// A base class for all SPH kernels, providing interface for computing kernel values and gradients.
/// All derived class must implement method <code>value</code> and
/// <code>grad</code>. Both function take SQUARED value of dimensionless distance q as a parameter. Function
/// value returns the kernel value, grad returns gradient DIVIDED BY q.
template <typename TDerived, Size D>
class Kernel : public Noncopyable {
public:
    Kernel() = default;

    /// Value of kernel at given point
    /// this should be called only once for a pair of particles as there is expensive division
    /// \todo Test this carefully before going any further
    /// \todo Potentially precompute the 3rd power ...
    INLINE Float value(const Vector& r, const Float h) const {
        ASSERT(h > 0._f);
        const Float hInv = 1._f / h;
        return pow<D>(hInv) * impl().valueImpl(getSqrLength(r) * sqr(hInv));
    }

    INLINE Vector grad(const Vector& r, const Float h) const {
        ASSERT(h > 0._f);
        const Float hInv = 1._f / h;
        return r * pow<D + 2>(hInv) * impl().gradImpl(getSqrLength(r) * sqr(hInv));
    }

private:
    const TDerived& impl() const {
        return static_cast<const TDerived&>(*this);
    }
};


/// A look-up table approximation of the kernel. Can be constructed from any SPH kernel.
template <Size D>
class LutKernel : public Kernel<LutKernel<D>, D> {
private:
    static constexpr Size NEntries = 40000;

    /*struct {
        Float values[NEntries + 4096 / sizeof(Float)];
        Float grads[NEntries + 4096 / sizeof(Float)];
    } storage;*/

    Float values[NEntries];
    Float grads[NEntries];

    Float rad = 0._f;
    Float radInvSqr;
    Float qSqrToIdx;

public:
    LutKernel() = default;

    template <typename TKernel>
    LutKernel(TKernel&& source) {
        rad = source.radius();

        // align to page size
        /// \todo
        /*values = storage.values;//(Float*)(((uint64_t(storage.values) / 4096) + 1) * 4096);
        grads = storage.grads; //(Float*)(((uint64_t(storage.grads) / 4096) + 1) * 4096);
        ASSERT((values - storage.values) * sizeof(Float) <= 4096);
        ASSERT((grads - storage.grads) * sizeof(Float) <= 4096);*/

        ASSERT(rad > 0._f);
        radInvSqr = 1._f / (rad * rad);
        qSqrToIdx = Float(NEntries) * radInvSqr;
        for (Size i = 0; i < NEntries; ++i) {
            const Float qSqr = Float(i) / qSqrToIdx;
            values[i] = source.valueImpl(qSqr);
            grads[i] = source.gradImpl(qSqr);
        }
        /// \todo re-normalize?
    }

    INLINE bool isInit() const {
        return rad > 0._f;
    }

    INLINE Float radius() const {
        return rad;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        ASSERT(qSqr >= 0.f);
        ASSERT(isInit());
        if (qSqr >= sqr(rad)) {
            // outside of kernel support
            return 0._f;
        }
        // linear interpolation of stored values
        const Float floatIdx = qSqrToIdx * qSqr;
        const Size idx1 = Size(floatIdx);
        ASSERT(idx1 < NEntries);
        const Size idx2 = idx1 + 1;
        const Float ratio = floatIdx - Float(idx1);
        ASSERT(ratio >= 0._f && ratio < 1._f);

        return values[idx1] * (1._f - ratio) + (idx2 < NEntries ? values[idx2] : 0._f) * ratio;
    }

    INLINE Float gradImpl(const Float qSqr) const {
        ASSERT(qSqr >= 0._f);
        ASSERT(isInit());
        if (qSqr >= sqr(rad)) {
            // outside of kernel support
            return 0._f;
        }
        const Float floatIdx = qSqrToIdx * qSqr;
        const Size idx1 = Size(floatIdx);
        ASSERT(unsigned(idx1) < unsigned(NEntries));
        const Size idx2 = idx1 + 1;
        const Float ratio = floatIdx - Float(idx1);
        ASSERT(ratio >= 0._f && ratio < 1._f);

        return grads[idx1] * (1._f - ratio) + (idx2 < NEntries ? grads[idx2] : 0._f) * ratio;
    }
};


/// A cubic spline (M4) kernel
template <Size D>
class CubicSpline : public Kernel<CubicSpline<D>, D> {
private:
    const Float normalization[3] = { 2._f / 3._f, 10._f / (7._f * PI), 1._f / PI };

public:
    CubicSpline() = default;

    INLINE Float radius() const {
        return 2._f;
    }

    // template <bool TApprox = false>
    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        ASSERT(q >= 0);
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
            return (1._f / q) * normalization[D - 1] * (-0.75f * pow<2>(2.f - q));
        }
        return 0._f;
    }
};

/// A fourth-order spline (M5) kernel
template <Size D>
class FourthOrderSpline : public Kernel<FourthOrderSpline<D>, D> {
private:
    const Float normalization[3] = { 1._f / 24._f, 96._f / (1199._f * PI), 1._f / (20._f * PI) };

public:
    FourthOrderSpline() = default;

    INLINE Float radius() const {
        return 2.5_f;
    }

    // template <bool TApprox = false>
    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        ASSERT(q >= 0);
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

/// Kernel proposed by Read et al. (2010) with improved stability. Only for 3 dimensions.
class CoreTriangle : public Kernel<CoreTriangle, 3> {
public:
    INLINE Float radius() const {
        return 1._f;
    }

    INLINE Float valueImpl(const Float qSqr) const {
        const Float q = sqrt(qSqr);
        const Float alpha = 1._f / 3._f;
        const Float beta = 1._f + 6._f * sqr(alpha) - 12._f * pow<3>(alpha);
        const Float normalization = 8._f / (PI * (6.4_f * pow<5>(alpha) - 16._f * pow<6>(alpha) + 1._f));
        if (q < alpha) {
            return normalization * ((-12._f * alpha + 18._f * sqr(alpha)) * q + beta);
        } else if (q < 0.5_f) {
            return normalization * (1._f - 6._f * q * q * (1._f - q));
        } else if (q < 1._f) {
            return normalization * 2._f * pow<3>(1._f - q);
        } else {
            return 0._f;
        }
    }

    INLINE Float gradImpl(const Float UNUSED(qSqr)) const {
        NOT_IMPLEMENTED;
    }
};

/// Gaussian kernel, clamped to zero at radius 5 (the error is therefore about exp(-5^2) = 10^-11).
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
        return 0.5_f * (kernel.value(r1 - r2, r1[H]) + kernel.value(r1 - r2, r2[H]));
    }

    INLINE Vector grad(const Vector& r1, const Vector& r2) const {
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
        return kernel.value(r1 - r2, 0.5_f * (r1[H] + r2[H]));
    }

    INLINE Vector grad(const Vector& r1, const Vector& r2) const {
        return kernel.grad(r1 - r2, 0.5_f * (r1[H] + r2[H]));
    }

    INLINE Float radius() const {
        return kernel.radius();
    }
};


NAMESPACE_SPH_END
