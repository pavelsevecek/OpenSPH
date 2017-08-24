#pragma once

/// \file Element.h
/// \brief Object converting quantity values of particles into colors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017


#include "gui/Factory.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "objects/utility/Value.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Particle.h"

NAMESPACE_SPH_BEGIN

class Particle;

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

        /// Checks if the element has been initialized.
        virtual bool isInitialized() const = 0;

        /// Returns the color of idx-th particle.
        virtual Color eval(const Size idx) const = 0;

        /// Returns the original value of the displayed quantity, or NOTHING if no such value exists.
        virtual Optional<Particle> getParticle(const Size idx) const = 0;

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
    VELOCITY = -1,             ///< Particle velocities
    ACCELERATION = -2,         ///< Acceleration of particles
    MOVEMENT_DIRECTION = -3,   ///< Projected direction of motion
    DISPLACEMENT = -4,         ///< Difference between current positions and initial position
    DENSITY_PERTURBATION = -5, ///< Relative difference of density and initial density (rho/rho0 - 1)
    BOUNDARY = -6,             ///< Shows boundary particles
};

/// Default element simply converting quantity value to color using defined palette. Vector and tensor
/// quantities are converted to floats using suitable norm.
template <typename Type>
class TypedElement : public Abstract::Element {
protected:
    QuantityId id;
    Palette palette;
    ArrayView<const Type> values;
    Array<Type> cached;

    TypedElement() = default;

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

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Color eval(const Size idx) const override {
        ASSERT(values);
        return palette(Detail::getElementValue(values[idx]));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(id, values[idx], idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }


    virtual std::string name() const override {
        return getMetadata(id).quantityName;
    }
};

/// Displays particle velocities.
class VelocityElement : public TypedElement<Vector> {
public:
    VelocityElement(const Range range) {
        palette = Factory::getPalette(ElementId::VELOCITY, range);
    }

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (source == ElementSource::CACHE_ARRAYS) {
            cached = copyable(storage.getDt<Vector>(QuantityId::POSITIONS));
            values = cached;
        } else {
            values = storage.getDt<Vector>(QuantityId::POSITIONS);
        }
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITIONS, values[idx]);
    }

    virtual std::string name() const override {
        return "Velocity";
    }
};

class AccelerationElement : public TypedElement<Vector> {
public:
    AccelerationElement(const Range range) {
        palette = Factory::getPalette(ElementId::ACCELERATION, range);
    }

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (source == ElementSource::CACHE_ARRAYS) {
            cached = copyable(storage.getD2t<Vector>(QuantityId::POSITIONS));
            values = cached;
        } else {
            values = storage.getD2t<Vector>(QuantityId::POSITIONS);
        }
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addD2t(QuantityId::POSITIONS, values[idx]);
    }

    virtual std::string name() const override {
        return "Acceleration";
    }
};

/// Shows direction of particle movement in color
class DirectionElement : public Abstract::Element {
private:
    Palette palette;
    Vector axis;
    Vector dir1, dir2;

    ArrayView<const Vector> values;
    Array<Vector> cached;

public:
    DirectionElement(const Vector& axis)
        : palette(Factory::getPalette(ElementId::MOVEMENT_DIRECTION, Range(0._f, 2._f * PI)))
        , axis(axis) {
        ASSERT(almostEqual(getLength(axis), 1._f));
        // compute 2 perpendicular directions
        Vector ref;
        if (almostEqual(axis, Vector(0._f, 0._f, 1._f)) || almostEqual(axis, Vector(0._f, 0._f, -1._f))) {
            ref = Vector(0._f, 1._f, 0._f);
        } else {
            ref = Vector(0._f, 0._f, 1._f);
        }
        dir1 = getNormalized(cross(axis, ref));
        dir2 = cross(axis, dir1);
        ASSERT(almostEqual(getLength(dir2), 1._f));
    }

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (source == ElementSource::CACHE_ARRAYS) {
            cached = copyable(storage.getDt<Vector>(QuantityId::POSITIONS));
            values = cached;
        } else {
            values = storage.getDt<Vector>(QuantityId::POSITIONS);
        }
    }

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Color eval(const Size idx) const override {
        ASSERT(values);
        const Vector projected = values[idx] - dot(values[idx], axis) * axis;
        const Float x = dot(projected, dir1);
        const Float y = dot(projected - x * dir1, dir2);
        const Float angle = PI + atan2(y, x);
        return palette(angle);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        // return velocity of the particle
        return Particle(idx).addDt(QuantityId::POSITIONS, values[idx]);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Direction";
    }
};


/// \note This does not have anything in common with QuantityId::DISPLACEMENT; it only depends on particle
/// positions and have nothing to do with stresses.
class DisplacementElement : public Abstract::Element {};

class DensityPerturbationElement : public Abstract::Element {
private:
    Palette palette;
    ArrayView<const Float> rho;
    Array<Float> cached;
    Array<Float> rho0;

public:
    DensityPerturbationElement(const Range range)
        : palette(Factory::getPalette(ElementId::DENSITY_PERTURBATION, range)) {}

    virtual void initialize(const Storage& storage, const ElementSource source) override {
        if (source == ElementSource::CACHE_ARRAYS) {
            cached = copyable(storage.getValue<Float>(QuantityId::DENSITY));
            rho = cached;
        } else {
            rho = storage.getValue<Float>(QuantityId::DENSITY);
        }
        rho0.resize(rho.size());
        for (Size i = 0; i < rho.size(); ++i) {
            rho0[i] = storage.getMaterialOfParticle(i)->getParam<Float>(BodySettingsId::DENSITY);
        }
    }

    virtual bool isInitialized() const override {
        return !rho.empty();
    }

    virtual Color eval(const Size idx) const override {
        ASSERT(rho);
        return palette(rho[idx] / rho0[idx] - 1.f);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::DENSITY, rho[idx] / rho0[idx] - 1.f, idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Delta Density";
    }
};

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

    virtual bool isInitialized() const override {
        return (detection == Detection::NORMAL_BASED && !normals.values.empty()) ||
               (detection == Detection::NEIGBOUR_THRESHOLD && !neighbours.values.empty());
    }

    virtual Color eval(const Size idx) const override {
        if (isBoundary(idx)) {
            return Color::red();
        } else {
            return Color::gray();
        }
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        // doesn't really make sense to assign some value to boundary
        return NOTHING;
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
