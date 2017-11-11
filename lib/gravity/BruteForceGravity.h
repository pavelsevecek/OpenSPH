#pragma once

/// \file BruteForceGravity.h
/// \brief Simple gravity solver evaluating all particle pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gravity/IGravity.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"
#include "sph/kernel/GravityKernel.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

/// \brief Computes gravitational acceleration by summing up forces from all particle pairs.
///
/// This implementation is not intended for high-performance code because of the O(N) complexity. Useful for
/// testing and debugging purposes.
class BruteForceGravity : public IGravity {
private:
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

    GravityLutKernel kernel;

public:
    /// Default-construced gravity, assuming point-like particles
    BruteForceGravity() {
        ASSERT(kernel.radius() == 0._f);
    }

    /// Constructs gravity using smoothing kernel
    BruteForceGravity(GravityLutKernel&& kernel)
        : kernel(std::move(kernel)) {}

    virtual void build(const Storage& storage) override {
        r = storage.getValue<Vector>(QuantityId::POSITIONS);
        m = storage.getValue<Float>(QuantityId::MASSES);
    }

    virtual void evalAll(ArrayView<Vector> dv, Statistics& UNUSED(stats)) const override {
        ASSERT(r.size() == dv.size());
        SymmetrizeSmoothingLengths<const GravityLutKernel&> actKernel(kernel);
        for (Size i = 0; i < dv.size(); ++i) {
            dv[i] += this->evalImpl(actKernel, r[i], i);
        }
    }

    virtual void evalAll(IScheduler& scheduler,
        ArrayView<Vector> dv,
        Statistics& UNUSED(stats)) const override {
        ASSERT(r.size() == dv.size());
        SymmetrizeSmoothingLengths<const GravityLutKernel&> actKernel(kernel);
        parallelFor(scheduler, 0, r.size(), 10, [&dv, &actKernel, this](const Size n1, const Size n2) {
            for (Size i = n1; i < n2; ++i) {
                dv[i] += this->evalImpl(actKernel, r[i], i);
            }
        });
    }

    virtual Vector eval(const Vector& r0, Statistics& UNUSED(stats)) const override {
        struct NoSymmetrization {
            const GravityLutKernel& kernel;

            INLINE Vector grad(const Vector& r1, const Vector& r2) const {
                return kernel.grad(r1 - r2, r1[H]);
            }
        };
        return this->evalImpl(NoSymmetrization{ kernel }, r0, Size(-1));
    }

private:
    template <typename TKernel>
    INLINE Vector evalImpl(const TKernel& actKernel, const Vector& r0, const Size idx) const {
        ASSERT(r && m);
        Vector a(0._f);
        // do 2 for loops to avoid the if
        for (Size i = 0; i < idx; ++i) {
            a += m[i] * actKernel.grad(r[i], r0);
        }
        for (Size i = idx + 1; i < r.size(); ++i) {
            a += m[i] * actKernel.grad(r[i], r0);
        }
        return Constants::gravity * a;
    }
};

NAMESPACE_SPH_END
