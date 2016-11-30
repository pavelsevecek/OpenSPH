#pragma once

#include "objects/containers/ArrayView.h"
#include "storage/QuantityKey.h"
#include "storage/Storage.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    template <typename TValue>
    class Integral : public Polymorphic {
    protected:
        std::shared_ptr<Storage> storage;

    public:
        Integral(const std::shared_ptr<Storage>& storage)
            : storage(storage) {}

        virtual TValue operator()() const = 0;

        virtual Float getVariance() const = 0;
    };
}

class TotalMomentum : public Abstract::Integral<Vector> {
public:
    TotalMomentum(const std::shared_ptr<Storage>& storage)
        : Abstract::Integral<Vector>(storage) {}

    virtual Vector operator()() const override {
        Vector total(0._f);
        ArrayView<const Vector> vs = storage->template dt<QuantityKey::R>();
        ArrayView<const Float> ms  = storage->template get<QuantityKey::M>();
        for (int i = 0; i < vs.size(); ++i) {
            total += ms[i] * vs[i];
        }
        return total;
    }

    virtual Float getVariance() const override {
        Vector total(0._f);
        Float totalSqr             = 0._f;
        ArrayView<const Vector> vs = storage->template dt<QuantityKey::R>();
        ArrayView<const Float> ms  = storage->template get<QuantityKey::M>();
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
    TotalAngularMomentum(const std::shared_ptr<Storage>& storage)
        : Abstract::Integral<Vector>(storage) {}

    virtual Vector operator()() const override {
        Vector total(0._f);
        ArrayView<const Vector> rs = storage->template get<QuantityKey::R>();
        ArrayView<const Vector> vs = storage->template dt<QuantityKey::R>();
        ArrayView<const Float> ms  = storage->template get<QuantityKey::M>();
        for (int i = 0; i < vs.size(); ++i) {
            total += ms[i] * cross(rs[i], vs[i]);
        }
        return total;
    }

    virtual Float getVariance() const override {
        Vector total(0._f);
        Float totalSqr             = 0._f;
        ArrayView<const Vector> rs = storage->template get<QuantityKey::R>();
        ArrayView<const Vector> vs = storage->template dt<QuantityKey::R>();
        ArrayView<const Float> ms  = storage->template get<QuantityKey::M>();
        for (int i = 0; i < vs.size(); ++i) {
            const Vector p = ms[i] * cross(rs[i], vs[i]);
            total += p;
            totalSqr += getSqrLength(p);
        }
        return totalSqr - getSqrLength(total);
    }
};

NAMESPACE_SPH_END
