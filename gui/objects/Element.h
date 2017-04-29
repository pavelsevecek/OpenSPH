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
        /// Initialize the element before by getting necessary quantities from storage. Must be called before
        /// \ref eval is called, every time step as ArrayViews taken from storage might be invalidated.
        /// \param storage Particle storage containing source data to be drawn.
        virtual void initialize(const Storage& storage, const bool clone) = 0;

        /// Returns the color of idx-th particle.
        virtual Color eval(const Size idx) const = 0;

        /// Returns recommended palette for drawing this element.
        virtual Palette getPalette() const = 0;

        /// Returns the name of the element, used when showing the element in the window and as filename
        /// suffix.
        virtual std::string name() const = 0;
    };
}

namespace Detail {
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
}

/// Default element simply converting quantity value to color using defined palette. Vector and tensor
/// quantities are converted to floats using suitable norm.
template <typename Type>
class TypedElement : public Abstract::Element {
private:
    QuantityId id;
    Palette palette;
    ArrayView<const Type> values;
    Array<Type> cached;

public:
    TypedElement(const QuantityId id, const Range range)
        : id(id)
        , palette(Palette::forQuantity(id, range)) {}

    virtual void initialize(const Storage& storage, const bool clone) override {
        if (clone) {
            cached = copyable(storage.getValue<Type>(id));
            values = cached;
        } else {
            values = storage.getValue<Type>(id);
        }
    }

    virtual Color eval(const Size idx) const override {
        ASSERT(values);
        return palette(Detail::getElementValue(values[idx]));
    }

    virtual Palette getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return getQuantityName(id);
    }
};

/// Displays particle velocities.
class VelocityElement : public Abstract::Element {
private:
    Palette palette;
    ArrayView<const Vector> values;
    Array<Vector> cached;

public:
    VelocityElement(const Range range)
        : palette(Palette::forQuantity(QuantityId::POSITIONS, range)) {}

    virtual void initialize(const Storage& storage, const bool clone) override {
        if (clone) {
            cached = copyable(storage.getDt<Vector>(QuantityId::POSITIONS));
            values = cached;
        } else {
            values = storage.getDt<Vector>(QuantityId::POSITIONS);
        }
    }

    virtual Color eval(const Size idx) const override {
        ASSERT(values);
        return palette(Detail::getElementValue(values[idx]));
    }

    virtual Palette getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Velocity";
    }
};


NAMESPACE_SPH_END
