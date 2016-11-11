#pragma once

#include "storage/Storage.h"
#include "storage/QuantityMap.h"
#include "objects/containers/ArrayView.h"
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

        virtual TValue compute() const = 0;
    };
}

class TotalMomentum : public Abstract::Integral<Vector> {
public:
    TotalMomentum(const std::shared_ptr<Storage>& storage)
        : Abstract::Integral<Vector>(storage) {}

    virtual Vector compute() const override {
        Vector total(0._f);
        ArrayView<const Vector> vs = storage->template dt<QuantityKey::R>();
        ArrayView<const Float> ms  = storage->template get<QuantityKey::M>();
        for (int i = 0; i < vs.size(); ++i) {
            total += ms[i] * vs[i];
        }
        return total;
    }
};

class TotalAngularMomentum : public Abstract::Integral<Vector> {
public:
    TotalAngularMomentum(const std::shared_ptr<Storage>& storage)
        : Abstract::Integral<Vector>(storage) {}

    virtual Vector compute() const override {
        Vector total               (0._f);
        ArrayView<const Vector> rs = storage->template get<QuantityKey::R>();
        ArrayView<const Vector> vs = storage->template dt<QuantityKey::R>();
        ArrayView<const Float> ms  = storage->template get<QuantityKey::M>();
        for (int i = 0; i < vs.size(); ++i) {
            total += ms[i] * cross(rs[i], vs[i]);
        }
        return total;
    }
};

NAMESPACE_SPH_END
