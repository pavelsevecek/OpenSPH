#pragma once

/// SPH kernels
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// A base class for all SPH kernels, providing interface for computing kernel values and gradients.
/// All derived class must implement method <code>value</code> and
/// <code>grad</code>. Both function take SQUARED value of dimensionless distance q as a parameter. Function
/// value returns the kernel value, grad returns gradient DIVIDED BY q.
template <class TDerived>
class Kernel : public Noncopyable {
private:
    const TDerived* kernel;

public:
    Kernel() {
        // static polymorphism to avoid calling (slow) virtual functions
        kernel = static_cast<TDerived*>(this);
    }

    /// Value of kernel at given point
    /// this should be called only once for a pair of particles as there is expensive division
    /// \todo Test this carefully before going any further
    /// \todo Potentially precompute the 3rd power ...
    /// \todo How to resolve dimensions for d!=3 ??
    //    template <bool TApprox = false>
    INLINE Float value(const Vector& r, const Float& h) const {
        return Math::pow<-3>(h) * kernel->valueImpl(getSqrLength(r) / (h * h));
    }

    //  template <bool TApprox = false>
    INLINE Vector grad(const Vector& r, const Float& h) const {
        return r * Math::pow<-5>(h) * kernel->gradImpl(getSqrLength(r) / (h * h));
    }
};


/// A look-up table approximation of the kernel. Can be constructed from any SPH kernel.
class LutKernel : public Kernel<LutKernel> {
private:
    static constexpr int NEntries = 40000;

    Float values[NEntries];
    Float grads[NEntries];
    Float rad = 0._f;
    Float radInvSqr;

public:
    LutKernel() = default;

    template <typename TKernel>
    LutKernel(TKernel&& source) {
        rad = source.radius();
        ASSERT(rad > 0._f);
        radInvSqr = 1._f / (rad * rad);
        for (int i = 0; i < NEntries; ++i) {
            const Float q = Float(i) / Float(NEntries) * Math::sqr(rad);
            values[i]     = source.valueImpl(q);
            grads[i]      = source.gradImpl(q);
        }
        /// \todo re-normalize?
    }

    LutKernel& operator=(LutKernel&& other) {
        std::swap(values, other.values);
        std::swap(grads, other.grads);
        rad       = other.rad;
        radInvSqr = other.radInvSqr;
        return *this;
    }

    INLINE bool isInit() const { return rad > 0.f; }

    INLINE Float radius() const { return rad; }

    // template <bool TApprox = false>
    INLINE Float valueImpl(const Float& qSqr) const {
        ASSERT(qSqr >= 0.f);
        // linear interpolation of stored values
        const Float floatIdx = Float(NEntries) * qSqr * radInvSqr;
        const int idx1       = floor(floatIdx);
        if (idx1 >= NEntries) {
            return 0._f;
        }
        const int idx2    = idx1 + 1;
        const Float ratio = floatIdx - Float(idx1);

        return values[idx1] * (1._f - ratio) + (idx2 < NEntries ? values[idx2] : 0._f) * ratio;
    }

    // template <bool TApprox = false>
    INLINE Float gradImpl(const Float& qSqr) const {
        ASSERT(qSqr >= 0._f);
        if (qSqr >= Math::sqr(rad)) {
            // outside of kernel support
            return 0._f;
        }
        const Float floatIdx = Float(NEntries) * qSqr * radInvSqr;
        const int idx1       = floor(floatIdx);
        ASSERT(unsigned(idx1) < unsigned(NEntries));
        const int idx2    = idx1 + 1;
        const Float ratio = floatIdx - Float(idx1);

        return grads[idx1] * (1._f - ratio) + (idx2 < NEntries ? grads[idx2] : 0._f) * ratio;
    }
};


/// A cubic spline (M4) kernel
class CubicSpline : public Kernel<CubicSpline> {
private:
    static constexpr Float normalization = 1._f / Math::PI;

public:
    CubicSpline() = default;

    INLINE Float radius() const { return 2._f; }

    // template <bool TApprox = false>
    INLINE Float valueImpl(const Float& qSqr) const {
        const Float q = Math::sqrt(qSqr);
        ASSERT(q >= 0);
        if (q < 1._f) {
            return normalization * (0.25_f * Math::pow<3>(2._f - q) - Math::pow<3>(1._f - q));
        }
        if (q < 2._f) {
            return normalization * (0.25_f * Math::pow<3>(2._f - q));
        }
        return 0._f; // compact within 2r radius
    }

    // template <bool TApprox = false>
    INLINE Float gradImpl(const Float& qSqr) const {
        const Float q = Math::sqrt(qSqr);
        if (q == 0) {
            return 0._f;
        }
        if (q < 1._f) {
            return (1._f / q) * normalization *
                   (-0.75_f * Math::pow<2>(2._f - q) + 3 * Math::pow<2>(1._f - q));
        }
        if (q < 2._f) {
            return (1._f / q) * normalization * (-0.75f * Math::pow<2>(2.f - q));
        }
        return 0._f;
    }
};

/*LutKernel makeKernel(const KernelEnum type) {
    switch (type) {
    case KernelEnum::CUBIC_SPLINE:
        return LutKernel(CubicSpline());
    default:
        return LutKernel();
    }
}*/

/// Symmetrization of the kernel with a respect to different smoothing lenths
/// Two possibilities - Symmetrized kernel W_ij = 0.5(W_i + W_j)
///                   - Symmetrized smoothing length h_ij = 0.5(h_i + h_j)
class SymW : public Object {
private:
    const LutKernel& kernel;

public:
    SymW(const LutKernel& kernel) : kernel(kernel) {}

    Float getValue(const Vector& r1, const Vector& r2) {
        return 0.5_f * (kernel.value(r1 - r2, r1[H]) + kernel.value(r1 - r2, r2[H]));
    }

    Vector getGrad(const Vector& r1, const Vector& r2) {
        return 0.5_f * (kernel.grad(r1 - r2, r1[H]) + kernel.grad(r1 - r2, r2[H]));
    }
};

class SymH : public Object {
private:
    const LutKernel& kernel;

public:
    SymH(const LutKernel& kernel) : kernel(kernel) {}

    Float getValue(const Vector& r1, const Vector& r2) {
        return kernel.value(r1 - r2, 0.5_f * (r1[H] + r2[H]));
    }

    Vector getGrad(const Vector& r1, const Vector& r2) {
        return kernel.grad(r1 - r2, 0.5_f * (r1[H] + r2[H]));
    }
};


NAMESPACE_SPH_END
