#pragma once

/// \file BruteForceGravity.h
/// \brief Simple gravity solver evaluating all particle pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gravity/IGravity.h"
#include "physics/Constants.h"
#include "quantities/Attractor.h"
#include "quantities/Storage.h"
#include "sph/kernel/GravityKernel.h"
#include "thread/ThreadLocal.h"

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
    Float G = Constants::gravity;

public:
    /// \brief Default-construced gravity, assuming point-like particles
    BruteForceGravity(const Float gravityContant = Constants::gravity)
        : G(gravityContant) {
        SPH_ASSERT(kernel.radius() == 0._f);
    }

    /// \brief Constructs gravity using smoothing kernel
    BruteForceGravity(GravityLutKernel&& kernel, const Float gravityContant = Constants::gravity)
        : kernel(std::move(kernel))
        , G(gravityContant) {}

    virtual void build(IScheduler& UNUSED(scheduler), const Storage& storage) override {
        r = storage.getValue<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual void evalSelfGravity(IScheduler& scheduler,
        ArrayView<Vector> dv,
        Statistics& UNUSED(stats)) const override {
        SPH_ASSERT(r.size() == dv.size());
        SymmetrizeSmoothingLengths<const GravityLutKernel&> symmetricKernel(kernel);
        parallelFor(scheduler, 0, r.size(), [&dv, &symmetricKernel, this](const Size i) {
            dv[i] += this->evalImpl(symmetricKernel, r[i], i);
        });
    }

    virtual void evalAttractors(IScheduler& scheduler,
        ArrayView<Attractor> attractors,
        ArrayView<Vector> dv) const override {
        SymmetrizeSmoothingLengths<const GravityLutKernel&> symmetricKernel(kernel);
        // attractor-particle interactions
        for (Attractor& a : attractors) {
            parallelFor(scheduler, 0, r.size(), [&dv, &a, this, &symmetricKernel](const Size i) {
                const Vector f = G * symmetricKernel.grad(r[i], setH(a.position, a.radius));
                dv[i] -= a.mass * f;
                a.acceleration += m[i] * f;
            });
        }
        // attractor-attractor interactions
        for (Size i = 0; i < attractors.size(); ++i) {
            for (Size j = i + 1; j < attractors.size(); ++j) {
                Attractor& a1 = attractors[i];
                Attractor& a2 = attractors[j];
                const Vector f =
                    G * symmetricKernel.grad(setH(a1.position, a1.radius), setH(a2.position, a2.radius));
                a1.acceleration -= attractors[j].mass * f;
                a2.acceleration += attractors[i].mass * f;
            }
        }
    }

    virtual Vector evalAcceleration(const Vector& r0) const override {
        struct NoSymmetrization {
            const GravityLutKernel& kernel;

            INLINE Vector grad(const Vector& r1, const Vector& r2) const {
                return kernel.grad(r1 - r2, r1[H]);
            }
        };
        return this->evalImpl(NoSymmetrization{ kernel }, r0, Size(-1));
    }

    virtual Float evalEnergy(IScheduler& scheduler, Statistics& UNUSED(stats)) const override {
        SymmetrizeSmoothingLengths<const GravityLutKernel&> actKernel(kernel);
        ThreadLocal<Float> energy(scheduler, 0._f);
        parallelFor(scheduler, energy, 0, r.size(), [&actKernel, this](const Size i, Float& e) {
            for (Size j = 0; j < m.size(); ++j) {
                if (i != j) {
                    e += m[i] * m[j] * actKernel.value(r[i], r[j]);
                }
            }
        });
        return 0.5_f * G * energy.accumulate();
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        return nullptr;
    }

private:
    template <typename TKernel>
    INLINE Vector evalImpl(const TKernel& actKernel, const Vector& r0, const Size idx) const {
        SPH_ASSERT(r && m);
        Vector a(0._f);

        if (idx != Size(-1)) {
            // do 2 for loops to avoid the if
            for (Size i = 0; i < idx; ++i) {
                a += m[i] * actKernel.grad(r[i], r0);
            }
            for (Size i = idx + 1; i < r.size(); ++i) {
                a += m[i] * actKernel.grad(r[i], r0);
            }
        } else {
            for (Size i = 0; i < r.size(); ++i) {
                a += m[i] * actKernel.grad(r[i], r0);
            }
        }
        return G * a;
    }
};

NAMESPACE_SPH_END
