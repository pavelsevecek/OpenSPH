#pragma once

/// Object for printing quantity values into output.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Value.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Column : public Polymorphic {
    public:
        virtual Value evaluate(Storage& storage, const Size particleIdx) const = 0;

        virtual std::string getName() const = 0;

        virtual ValueEnum getType() const = 0;
    };
}

/// Returns values of given quantity as stored in storage.
template <typename TValue>
class ValueColumn : public Abstract::Column {
private:
    QuantityId id;

public:
    ValueColumn(const QuantityId id)
        : id(id) {}

    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getValue<TValue>(id);
        return value[particleIdx];
    }

    virtual std::string getName() const override {
        return getQuantityName(id);
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// Returns first derivatives of given quantity as stored in storage. Quantity must contain derivative,
/// checked by assert.
template <typename TValue>
class DerivativeColumn : public Abstract::Column {
private:
    QuantityId id;

public:
    DerivativeColumn(const QuantityId id)
        : id(id) {}

    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getDt<TValue>(id);
        return value[particleIdx];
    }

    virtual std::string getName() const override {
        return getDerivativeName(id);
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// Returns second derivatives of given quantity as stored in storage. Quantity must contain second
/// derivative, checked by assert.
template <typename TValue>
class SecondDerivativeColumn : public Abstract::Column {
private:
    QuantityId id;

public:
    SecondDerivativeColumn(const QuantityId id)
        : id(id) {}

    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getAll<TValue>(id)[2];
        return value[particleIdx];
    }

    virtual std::string getName() const override {
        NOT_IMPLEMENTED; // currently never used
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// Returns smoothing lengths of particles
class SmoothingLengthColumn : public Abstract::Column {
public:
    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const Vector> value = storage.getValue<Vector>(QuantityId::POSITIONS);
        return value[particleIdx][H];
    }

    virtual std::string getName() const override {
        return "Smoothing length";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};

/// Prints actual values of scalar damage, as damage is stored in storage as third roots.
/// Can be used for both scalar and tensor damage.
template <typename TValue>
class DamageColumn : public Abstract::Column {
public:
    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getValue<TValue>(QuantityId::DAMAGE);
        return pow<3>(value[particleIdx]);
    }

    virtual std::string getName() const override {
        return "Damage";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};

/// Helper column printing particle numbers.
class ParticleNumberColumn : public Abstract::Column {
    virtual Value evaluate(Storage& UNUSED(storage), const Size particleIdx) const override {
        return particleIdx;
    }

    virtual std::string getName() const override {
        return "Particle index";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::INDEX;
    }
};

NAMESPACE_SPH_END
