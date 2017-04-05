#pragma once

/// Object for printing quantity values into output.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Value.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Column : public Polymorphic {
    public:
        /// Returns the value of the output column for given particle.
        virtual Value evaluate(Storage& storage, const Size particleIdx) const = 0;

        /// Reads the value of the column and saves it into the storage, if possible.
        virtual void accumulate(Storage& storage, const Value value, const Size particleIdx) const = 0;

        virtual std::string getName() const = 0;

        virtual ValueEnum getType() const = 0;
    };
}

/// Returns values of given quantity as stored in storage.
template <typename TValue>
class ValueColumn : public Abstract::Column {
private:
    QuantityIds id;

public:
    ValueColumn(const QuantityIds id)
        : id(id) {}

    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getValue<TValue>(id);
        return value[particleIdx];
    }

    virtual void accumulate(Storage& storage, const Value value, const Size particleIdx) const override {
        Array<TValue>& array = storage.getValue<TValue>(id);
        array.resize(particleIdx + 1); /// \todo must also resize derivaties, or avoid resizing
        array[particleIdx] = value.get<TValue>();
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
    QuantityIds id;

public:
    DerivativeColumn(const QuantityIds id)
        : id(id) {}

    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getDt<TValue>(id);
        return value[particleIdx];
    }

    virtual void accumulate(Storage& storage, const Value value, const Size particleIdx) const override {
        Array<TValue>& array = storage.getDt<TValue>(id);
        array.resize(particleIdx + 1);
        array[particleIdx] = value.get<TValue>();
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
    QuantityIds id;

public:
    SecondDerivativeColumn(const QuantityIds id)
        : id(id) {}

    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const TValue> value = storage.getAll<TValue>(id)[2];
        return value[particleIdx];
    }

    virtual void accumulate(Storage& storage, const Value value, const Size particleIdx) const override {
        Array<TValue>& array = storage.getAll<TValue>(id)[2];
        array.resize(particleIdx + 1);
        array[particleIdx] = value.get<TValue>();
    }

    virtual std::string getName() const override {
        return getSecondDerivativeName(id);
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

/// Returns smoothing lengths of particles
class SmoothingLengthColumn : public Abstract::Column {
public:
    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const Vector> value = storage.getValue<Vector>(QuantityIds::POSITIONS);
        return value[particleIdx][H];
    }

    virtual void accumulate(Storage& storage, const Value value, const Size particleIdx) const override {
        Array<Vector>& array = storage.getValue<Vector>(QuantityIds::POSITIONS);
        array.resize(particleIdx + 1);
        array[particleIdx][H] = value.get<Float>();
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
        ArrayView<const TValue> value = storage.getValue<TValue>(QuantityIds::DAMAGE);
        return pow<3>(value[particleIdx]);
    }

    virtual void accumulate(Storage& storage, const Value value, const Size particleIdx) const override {
        Array<TValue>& array = storage.getValue<TValue>(QuantityIds::DAMAGE);
        array.resize(particleIdx + 1);
        array[particleIdx] = root<3>(value.get<TValue>());
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
public:
    virtual Value evaluate(Storage& UNUSED(storage), const Size particleIdx) const override {
        return particleIdx;
    }

    virtual void accumulate(Storage&, const Value, const Size) const override {
        // do nothing
    }

    virtual std::string getName() const override {
        return "Particle index";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::INDEX;
    }
};

/// Helper column printing current run time. This value is the same for every particle.
class TimeColumn : public Abstract::Column {
private:
    Statistics* stats;

public:
    TimeColumn(Statistics* stats)
        : stats(stats) {}

    virtual Value evaluate(Storage& UNUSED(storage), const Size UNUSED(particleIdx)) const override {
        return stats->get<Float>(StatisticsIds::TOTAL_TIME);
    }

    virtual void accumulate(Storage&, const Value, const Size) const override {
        // do nothing
    }

    virtual std::string getName() const override {
        return "Time";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};


NAMESPACE_SPH_END
