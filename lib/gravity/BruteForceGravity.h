#pragma once

/// \file BruteForceGravity.h
/// \brief Simple gravity solver evaluating all particle pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/AbstractGravity.h"
#include "physics/Constants.h"
#include "sph/kernel/GravityKernel.h"

NAMESPACE_SPH_BEGIN

/// \brief Computes gravitational acceleration by summing up forces from all particle pairs.
///
/// This implementation is not intended for high-performance code because of the O(N) complexity. Useful for
/// testing and debugging purposes.
class BruteForceGravity : public Abstract::Gravity {
private:
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

    GravityLutKernel kernel;

public:
    /// Default-construced gravity, assuming point-like particles
    BruteForceGravity() {
        ASSERT(kernel.closeRadius() == 0._f);
    }

    /// Constructs gravity using smoothing kernel
    BruteForceGravity(GravityLutKernel&& kernel)
        : kernel(std::move(kernel)) {}

    virtual void build(ArrayView<const Vector> points, ArrayView<const Float> masses) override {
        r = points;
        m = masses;
    }

    virtual Vector eval(const Size idx) override {
        return this->evalImpl(r[idx], idx);
    }

    virtual Vector eval(const Vector& r0) override {
        return this->evalImpl(r0, Size(-1));
    }

private:
    INLINE Vector evalImpl(const Vector& r0, const Size idx) {
        ASSERT(r && m);
        Vector a(0._f);
        ASSERT(r0[H] > 0._f, r0[H]);
        for (Size i = 0; i < r.size(); ++i) {
            if (i == idx) {
                continue;
            }
            a += m[i] * kernel.grad(r[i] - r0, r0[H]);
        }
        return Constants::gravity * a;
    }
};

NAMESPACE_SPH_END
