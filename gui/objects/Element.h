#pragma once

/// \file Element.h
/// \brief Object converting quantity values of particles into colors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017


#include "gui/Factory.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// Source data used for element drawing
enum class ElementSource {
    /// Necessary data are cached withing the array and can be safely accessed during the run
    CACHE_ARRAYS,
    /// Element only saves a references to the storage, which can be invalidated during the run. Can be only
    /// used for drawing inbetween timesteps or after run ends.
    POINTER_TO_STORAGE

};

namespace Abstract {
    class Element : public Polymorphic {
    public:
        /// Initialize the element before by getting necessary quantities from storage. Must be called before
        /// \ref eval is called, every time step as ArrayViews taken from storage might be invalidated.
        /// \param storage Particle storage containing source data to be drawn.
        virtual void initialize(const Storage& storage, const ElementSource source) = 0;

        /// Returns the color of idx-th particle.
        virtual Color eval(const Size idx) const = 0;

        /// Returns recommended palette for drawing this element, or NOTHING in case there is no palette.
        virtual Optional<Palette> getPalette() const = 0;

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

/// Special elements that do not directly correspond to quantities, must have strictly negative values.
/// Function taking ElementId as an argument also acceps QuantityId casted to ElementId, interpreting as
/// TypedElement with given quantity ID.
enum class ElementId {
    VELOCITY = -1,     ///< Particle velocities
    DIRECTION = -2,    ///< Projected direction of motion
    DISPLACEMENT = -3, ///< Difference between current positions and initial positions
};

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
        , palette(Factory::getPalette(ElementId(id), range)) {}

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (source == ElementSource::CACHE_ARRAYS) {
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

    virtual Optional<Palette> getPalette() const override {
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
        : palette(Factory::getPalette(ElementId(QuantityId::POSITIONS), range)) {}

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (source == ElementSource::CACHE_ARRAYS) {
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

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Velocity";
    }
};

/// Shows direction of particle movement in color
class DirectionElement : public Abstract::Element {
private:
    Palette palette;
    Vector axis;

    ArrayView<const Vector> r, v;
    struct {
        Array<Vector> r, v;
    } cached;

public:
    DirectionElement(const Vector& axis)
        : palette(Factory::getPalette(ElementId::DIRECTION, Range(0._f, 2._f * PI)))
        , axis(axis) {}
};


/// \note This does not have anything in common with QuantityId::DISPLACEMENT; it only depends on particle
/// positions and have nothing to do with stresses.
class DisplacementElement : public Abstract::Element {};

/// Shows boundary elements
class BoundaryElement : public Abstract::Element {
public:
    enum class Detection {
        /// Particles with fewer neighbours are considered boundary. Not suitable if number of neighbours is
        /// enforced by adapting smoothing length. Note that increasing the threshold adds more particles into
        /// the boundary.
        NEIGBOUR_THRESHOLD,

        /// Boundary is determined by relative position vectors approximating surface normal. Has higher
        /// overhead, but does not depend sensitively on number of neighbours. Here, increasing the threshold
        /// leads to fewer boundary particles.
        NORMAL_BASED,
    };

private:
    Detection detection;

    struct {
        ArrayView<const Vector> values;
        Array<Vector> cached;
        Float threshold;
    } normals;

    struct {
        ArrayView<const Size> values;
        Array<Size> cached;
        Size threshold;
    } neighbours;

public:
    BoundaryElement(const Detection detection, const Float threshold = 15._f)
        : detection(detection) {
        if (detection == Detection::NEIGBOUR_THRESHOLD) {
            neighbours.threshold = Size(threshold);
        } else {
            normals.threshold = threshold;
        }
    }

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (detection == Detection::NORMAL_BASED) {
            if (source == ElementSource::CACHE_ARRAYS) {
                normals.cached = copyable(storage.getValue<Vector>(QuantityId::SURFACE_NORMAL));
                normals.values = normals.cached;
            } else {
                normals.values = storage.getValue<Vector>(QuantityId::SURFACE_NORMAL);
            }
        } else {
            if (source == ElementSource::CACHE_ARRAYS) {
                neighbours.cached = copyable(storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT));
                neighbours.values = neighbours.cached;
            } else {
                neighbours.values = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
            }
        }
    }

    virtual Color eval(const Size idx) const override {
        if (isBoundary(idx)) {
            return Color::red();
        } else {
            return Color::gray();
        }
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual std::string name() const override {
        return "Boundary";
    }

private:
    bool isBoundary(const Size idx) const {
        switch (detection) {
        case Detection::NEIGBOUR_THRESHOLD:
            ASSERT(neighbours.values);
            return neighbours.values[idx] < neighbours.threshold;
        case Detection::NORMAL_BASED:
            ASSERT(normals.values);
            return getLength(normals.values[idx]) > normals.threshold;
        default:
            NOT_IMPLEMENTED;
        }
    }
};


NAMESPACE_SPH_END
