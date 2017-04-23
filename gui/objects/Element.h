#pragma once

/// Object converting quantity values of particles into colors.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz


#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Element : public Polymorphic {
    public:
        /// Returns the color of idx-th particle.
        virtual Color eval(const Size idx) const = 0;

        virtual Palette getPalette() const = 0;
    };
}

template <typename Type>
INLINE float getElementValue(const Type& value) {
    return value;
}
template <>
INLINE float getElementValue(const Vector& value) {
    return getLength(value);
}
template <>
INLINE float getElementValue(const TracelessTensor& value) {
    return sqrt(ddot(value, value));
}

/// Default element simply converting quantity value to color using defined palette. Vector and tensor
/// quantities are converted to floats using suitable norm.
template <typename Type>
class TypedElement : public Abstract::Element {
private:
    ArrayView<const Type> values;
    Palette palette;

public:
    TypedElement(Storage& storage, const QuantityId id, const Range range)
        : palette(Palette::forQuantity(id, range)) {
        values = storage.getValue<Type>(id);
    }

    virtual Color eval(const Size idx) const override {
        return palette(getElementValue(values[idx]));
    }

    virtual Palette getPalette() const override {
        return palette;
    }
};

/// Displays particle velocities.
class VelocityElement : public Abstract::Element {
private:
    ArrayView<const Vector> values;
    Palette palette;

public:
    VelocityElement(Storage& storage, const Range range)
        : palette(Palette::forQuantity(QuantityId::POSITIONS, range)) {
        values = storage.getDt<Vector>(QuantityId::POSITIONS);
    }

    virtual Color eval(const Size idx) const override {
        return palette(getElementValue(values[idx]));
    }

    virtual Palette getPalette() const override {
        return palette;
    }
};


NAMESPACE_SPH_END
