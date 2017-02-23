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

    virtual std::string getName() const override {
        return getQuantityName(id);
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

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

    virtual std::string getName() const override {
        return getDerivativeName(id);
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

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

    virtual std::string getName() const override {
        NOT_IMPLEMENTED; // currently never used
    }

    virtual ValueEnum getType() const override {
        return GetValueEnum<TValue>::type;
    }
};

class SmoothingLengthColumn : public Abstract::Column {
public:
    virtual Value evaluate(Storage& storage, const Size particleIdx) const override {
        ArrayView<const Vector> value = storage.getValue<Vector>(QuantityIds::POSITIONS);
        return value[particleIdx][H];
    }

    virtual std::string getName() const override {
        return "Smoothing length";
    }

    virtual ValueEnum getType() const override {
        return ValueEnum::SCALAR;
    }
};

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

namespace Factory {
    template <typename TValue>
    INLINE std::unique_ptr<Abstract::Column> getValueColumn(const QuantityIds id) {
        return std::make_unique<ValueColumn<TValue>>(id);
    }

    template <typename TValue>
    INLINE std::unique_ptr<Abstract::Column> getDerivativeColumn(const QuantityIds id) {
        return std::make_unique<DerivativeColumn<TValue>>(id);
    }

    INLINE std::unique_ptr<Abstract::Column> getVelocityColumn() {
        return getDerivativeColumn<Vector>(QuantityIds::POSITIONS);
    }

    INLINE std::unique_ptr<Abstract::Column> getSmoothingLengthColumn() {
        return std::make_unique<SmoothingLengthColumn>();
    }
}

NAMESPACE_SPH_END
