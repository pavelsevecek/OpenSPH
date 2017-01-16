#pragma once

#include "objects/containers/ArrayView.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    template <typename TValue>
    class Integral : public Polymorphic {
    public:
        virtual TValue get(Storage& storage) const = 0;

        virtual Float getVariance(Storage& storage) const = 0;
    };
}

class TotalMomentum : public Abstract::Integral<Vector> {
public:
    virtual Vector get(Storage& storage) const override {
        Vector total(0._f);
        ArrayView<const Vector> rs, vs, dvs;
        tie(rs, vs, dvs) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityIds::MASSES);
        for (Size i = 0; i < vs.size(); ++i) {
            total += ms[i] * vs[i];
        }
        return total;
    }

    virtual Float getVariance(Storage& storage) const override {
        Vector total(0._f);
        Float totalSqr = 0._f;
        ArrayView<const Vector> rs, vs, dvs;
        tie(rs, vs, dvs) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityIds::MASSES);
        for (Size i = 0; i < vs.size(); ++i) {
            const Vector p = ms[i] * vs[i];
            total += p;
            totalSqr += getSqrLength(p);
        }
        return totalSqr - getSqrLength(total);
    }
};


class TotalAngularMomentum : public Abstract::Integral<Vector> {
public:
    virtual Vector get(Storage& storage) const override {
        Vector total(0._f);
        ArrayView<const Vector> rs, vs, dvs;
        tie(rs, vs, dvs) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityIds::MASSES);
        for (Size i = 0; i < vs.size(); ++i) {
            total += ms[i] * cross(rs[i], vs[i]);
        }
        return total;
    }

    virtual Float getVariance(Storage& storage) const override {
        Vector total(0._f);
        Float totalSqr = 0._f;
        ArrayView<const Vector> rs, vs, dvs;
        tie(rs, vs, dvs) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityIds::MASSES);
        for (Size i = 0; i < vs.size(); ++i) {
            const Vector p = ms[i] * cross(rs[i], vs[i]);
            total += p;
            totalSqr += getSqrLength(p);
        }
        return totalSqr - getSqrLength(total);
    }
};

class TotalEnergy : public Abstract::Integral<Float> {
public:
    virtual Float get(Storage& storage) const override {
        Float total = 0._f;
        ArrayView<const Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
        ArrayView<const Float> u = storage.getValue<Float>(QuantityIds::ENERGY);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityIds::MASSES);
        for (Size i = 0; i < v.size(); ++i) {
            total += 0.5_f * m[i] * getSqrLength(v[i]) + m[i] * u[i];
        }
        return total;
    }

    virtual Float getVariance(Storage&) const override {
        NOT_IMPLEMENTED;
    }
};

// debugging
class AvgDeviatoricStress : public Abstract::Integral<TracelessTensor> {
public:
    virtual TracelessTensor get(Storage& storage) const override {
        TracelessTensor avg = 0.f;
        ArrayView<const TracelessTensor> ds = storage.getAll<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS)[1];
        for (Size i = 0; i < ds.size(); ++i) {
            avg += ds[i];
        }
        return avg / ds.size();
    }

    virtual Float getVariance(Storage&) const override {
        NOT_IMPLEMENTED;
    }
};

NAMESPACE_SPH_END
