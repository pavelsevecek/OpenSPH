#pragma once

/// \file SoftenedBruteForceGravity.h
/// \brief Direct gravity solver with fixed Plummer softening

#include "gravity/IGravity.h"
#include "quantities/Attractor.h"
#include "quantities/Storage.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

class SoftenedBruteForceGravity : public IGravity {
private:
    ArrayView<const Vector> r;
    ArrayView<const Float> m;
    Float G;
    Float eps;

public:
    SoftenedBruteForceGravity(const Float gravityConstant, const Float softening)
        : G(gravityConstant)
        , eps(softening) {
        SPH_ASSERT(eps >= 0._f);
    }

    virtual void build(IScheduler& UNUSED(scheduler), const Storage& storage) override {
        r = storage.getValue<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual void evalSelfGravity(IScheduler& scheduler,
        ArrayView<Vector> dv,
        Statistics& UNUSED(stats)) const override {
        parallelFor(scheduler, 0, r.size(), [this, &dv](const Size i) { dv[i] += this->evalImpl(r[i], i); });
    }

    virtual void evalAttractors(IScheduler& scheduler,
        ArrayView<Attractor> attractors,
        ArrayView<Vector> dv) const override {
        parallelFor(scheduler, 0, r.size(), [this, &attractors, &dv](const Size i) {
            for (Attractor& a : attractors) {
                const Vector dr = a.position - r[i];
                const Float inv = 1._f / pow(getSqrLength(dr) + sqr(eps), 1.5_f);
                const Vector acc = G * a.mass * dr * inv;
                dv[i] += acc;
                a.acceleration -= m[i] * acc / a.mass;
            }
        });
    }

    virtual Vector evalAcceleration(const Vector& r0) const override {
        return this->evalImpl(r0, Size(-1));
    }

    virtual Float evalEnergy(IScheduler& scheduler, Statistics& UNUSED(stats)) const override {
        ThreadLocal<Float> energy(scheduler, 0._f);
        parallelFor(scheduler, energy, 0, r.size(), [this](const Size i, Float& e) {
            for (Size j = 0; j < r.size(); ++j) {
                if (i == j) {
                    continue;
                }
                const Vector dr = r[j] - r[i];
                e -= G * m[i] * m[j] / sqrt(getSqrLength(dr) + sqr(eps));
            }
        });
        return 0.5_f * energy.accumulate();
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        return nullptr;
    }

private:
    INLINE Vector evalImpl(const Vector& r0, const Size idx) const {
        Vector a(0._f);
        for (Size i = 0; i < r.size(); ++i) {
            if (i == idx) {
                continue;
            }
            const Vector dr = r[i] - r0;
            a += m[i] * dr / pow(getSqrLength(dr) + sqr(eps), 1.5_f);
        }
        return G * a;
    }
};

NAMESPACE_SPH_END
