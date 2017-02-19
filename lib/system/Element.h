#pragma once

/// Object for printing quantity values into output.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Value.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Element : public Polymorphic {
    public:
        virtual Value evaluate(Storage& storage, const Size particleIdx) const = 0;

        virtual std::string getName() const = 0;

        virtual ValueEnum getType() const = 0;
    };
}

template <typename TValue>
class ValueElement : public Abstract::Element {
private:
    QuantityIds id;

public:
    ValueElement(const QuantityIds id)
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
class DerivativeElement : public Abstract::Element {
private:
    QuantityIds id;

public:
    DerivativeElement(const QuantityIds id)
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
class SecondDerivativeElement : public Abstract::Element {
private:
    QuantityIds id;

public:
    SecondDerivativeElement(const QuantityIds id)
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

class SmoothingLengthElement : public Abstract::Element {
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

class ParticleNumberElement : public Abstract::Element {
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
    INLINE std::unique_ptr<Abstract::Element> getValueElement(const QuantityIds id) {
        return std::make_unique<ValueElement<TValue>>(id);
    }

    template <typename TValue>
    INLINE std::unique_ptr<Abstract::Element> getDerivativeElement(const QuantityIds id) {
        return std::make_unique<DerivativeElement<TValue>>(id);
    }

    INLINE std::unique_ptr<Abstract::Element> getVelocityElement() {
        return getDerivativeElement<Vector>(QuantityIds::POSITIONS);
    }

    INLINE std::unique_ptr<Abstract::Element> getSmoothingLengthElement() {
        return std::make_unique<SmoothingLengthElement>();
    }
}

NAMESPACE_SPH_END
