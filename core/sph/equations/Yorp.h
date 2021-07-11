#pragma once

#include "post/Analysis.h"
#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

class YorpSpinup : public IEquationTerm {
private:
    Float rate;

    Size stepCounter = 0;
    static constexpr Size recomputationPeriod = 20;

    Array<Size> idxs;


public:
    explicit YorpSpinup(const Float rate)
        : rate(rate) {}

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage) override {
        if (idxs.empty() || (stepCounter % recomputationPeriod == 0)) {
            idxs = Post::findLargestComponent(storage, 2._f, Post::ComponentFlag::OVERLAP);
        }
        stepCounter++;

        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        Vector omega = Post::getAngularFrequency(m, r, v, idxs);
        if (getSqrLength(omega) < 1.e-6_f) {
            omega = Vector(0._f, 0._f, 1._f);
        }
        const Vector dw = getNormalized(omega) * rate;

        for (Size i = 0; i < r.size(); ++i) {
            dv[i] += cross(dw, r[i]);
        }
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};

NAMESPACE_SPH_END
