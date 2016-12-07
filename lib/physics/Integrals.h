#pragma once

#include "objects/containers/ArrayView.h"
#include "storage/QuantityKey.h"
#include "storage/Storage.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    template <typename TValue>
    class Integral : public Polymorphic {
    public:
        virtual TValue operator()(Storage& storage) const = 0;

        virtual Float getVariance(Storage& storage) const = 0;
    };
}

class TotalMomentum : public Abstract::Integral<Vector> {
public:
    virtual Vector operator()(Storage& storage) const override {
        Vector total(0._f);
        ArrayView<const Vector> rs, vs, dvs;
        tieToArray(rs, vs, dvs) = storage.getAll<Vector>(QuantityKey::R);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityKey::M);
        for (int i = 0; i < vs.size(); ++i) {
            total += ms[i] * vs[i];
        }
        return total;
    }

    virtual Float getVariance(Storage& storage) const override {
        Vector total(0._f);
        Float totalSqr = 0._f;
        ArrayView<const Vector> rs, vs, dvs;
        tieToArray(rs, vs, dvs) = storage.getAll<Vector>(QuantityKey::R);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityKey::M);
        for (int i = 0; i < vs.size(); ++i) {
            const Vector p = ms[i] * vs[i];
            total += p;
            totalSqr += getSqrLength(p);
        }
        return totalSqr - getSqrLength(total);
    }
};


class TotalAngularMomentum : public Abstract::Integral<Vector> {
public:
    virtual Vector operator()(Storage& storage) const override {
        Vector total(0._f);
        ArrayView<const Vector> rs, vs, dvs;
        tieToArray(rs, vs, dvs) = storage.getAll<Vector>(QuantityKey::R);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityKey::M);
        for (int i = 0; i < vs.size(); ++i) {
            total += ms[i] * cross(rs[i], vs[i]);
        }
        return total;
    }

    virtual Float getVariance(Storage& storage) const override {
        Vector total(0._f);
        Float totalSqr = 0._f;
        ArrayView<const Vector> rs, vs, dvs;
        tieToArray(rs, vs, dvs) = storage.getAll<Vector>(QuantityKey::R);
        ArrayView<const Float> ms = storage.getValue<Float>(QuantityKey::M);
        for (int i = 0; i < vs.size(); ++i) {
            const Vector p = ms[i] * cross(rs[i], vs[i]);
            total += p;
            totalSqr += getSqrLength(p);
        }
        return totalSqr - getSqrLength(total);
    }
};

NAMESPACE_SPH_END
