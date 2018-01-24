#pragma once

/// \file Colorizer.h
/// \brief Object converting quantity values of particles into colors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018


#include "gui/Factory.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "objects/utility/Dynamic.h"
#include "quantities/IMaterial.h"
#include "quantities/Particle.h"

NAMESPACE_SPH_BEGIN

class Particle;

/// Source data used for colorizer drawing
enum class ColorizerSource {
    /// Necessary data are cached withing the array and can be safely accessed during the run
    CACHE_ARRAYS,
    /// Colorizer only saves a references to the storage, which can be invalidated during the run. Can be only
    /// used for drawing inbetween timesteps or after run ends.
    POINTER_TO_STORAGE

};

/// \brief Interface for objects assigning colors to particles.
///
/// Used to add a layer of abstraction between quantity values and displayed colors, allowing to visualize
/// various information that isn't directly stored as quantity, like relative values of quantitiees, angular
/// dependence of velocities, etc. Usually though, one wants to display raw quantity values, which can be
/// accomplished by \ref TypedColorizer.
class IColorizer : public Polymorphic {
public:
    /// Initialize the colorizer before by getting necessary quantities from storage. Must be called before
    /// \ref eval is called, every time step as ArrayViews taken from storage might be invalidated.
    /// \param storage Particle storage containing source data to be drawn.
    virtual void initialize(const Storage& storage, const ColorizerSource source) = 0;

    /// Checks if the colorizer has been initialized.
    virtual bool isInitialized() const = 0;

    /// Returns the color of idx-th particle.
    virtual Color eval(const Size idx) const = 0;

    /// Returns the original value of the displayed quantity, or NOTHING if no such value exists.
    virtual Optional<Particle> getParticle(const Size idx) const = 0;

    /// Returns recommended palette for drawing this colorizer, or NOTHING in case there is no palette.
    virtual Optional<Palette> getPalette() const = 0;

    /// Returns the name of the colorizer, used when showing the colorizer in the window and as filename
    /// suffix.
    virtual std::string name() const = 0;
};


namespace Detail {
    template <typename Type>
    INLINE float getColorizerValue(const Type& value) {
        ASSERT(isReal(value));
        return value;
    }
    template <>
    INLINE float getColorizerValue(const Vector& value) {
        const Float result = getLength(value);
        ASSERT(isReal(result), value);
        return result;
    }
    template <>
    INLINE float getColorizerValue(const TracelessTensor& value) {
        const Float result = sqrt(ddot(value, value));
        ASSERT(isReal(result), value);
        return result;
    }
    template <>
    INLINE float getColorizerValue(const SymmetricTensor& value) {
        const Float result = sqrt(ddot(value, value));
        ASSERT(isReal(result), value);
        return result;
    }
} // namespace Detail

/// Special colorizers that do not directly correspond to quantities, must have strictly negative values.
/// Function taking ColorizerId as an argument also acceps QuantityId casted to ColorizerId, interpreting as
/// TypedColorizer with given quantity ID.
enum class ColorizerId {
    VELOCITY = -1,             ///< Particle velocities
    ACCELERATION = -2,         ///< Acceleration of particles
    MOVEMENT_DIRECTION = -3,   ///< Projected direction of motion
    COROTATING_VELOCITY = -4,  ///< Velocities with a respect to the rotating body
    DISPLACEMENT = -5,         ///< Difference between current positions and initial position
    DENSITY_PERTURBATION = -6, ///< Relative difference of density and initial density (rho/rho0 - 1)
    YIELD_REDUCTION = -7,      ///< Reduction of stress tensor due to yielding (1 - f_vonMises)
    RADIUS = -8,               ///< Radii/smoothing lenghts of particles
    BOUNDARY = -9,             ///< Shows boundary particles
    ID = -10,                  ///< Each particle drawn with different color
};

/// Default colorizer simply converting quantity value to color using defined palette. Vector and tensor
/// quantities are converted to floats using suitable norm.
template <typename Type>
class TypedColorizer : public IColorizer {
protected:
    QuantityId id;
    Palette palette;
    ArrayView<const Type> values;
    Array<Type> cached;

    TypedColorizer() = default;

public:
    TypedColorizer(const QuantityId id, const Interval range)
        : TypedColorizer(id, ColorizerId(id), range) {}

    TypedColorizer(const QuantityId id, const ColorizerId colorizerId, const Interval range)
        : id(id)
        , palette(Factory::getPalette(colorizerId, range)) {}

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
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
        return palette(Detail::getColorizerValue(values[idx]));
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
class VelocityColorizer : public TypedColorizer<Vector> {
public:
    VelocityColorizer(const Interval range) {
        palette = Factory::getPalette(ColorizerId::VELOCITY, range);
    }

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
            cached = copyable(storage.getDt<Vector>(QuantityId::POSITION));
            values = cached;
        } else {
            values = storage.getDt<Vector>(QuantityId::POSITION);
        }
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, values[idx]);
    }

    virtual std::string name() const override {
        return "Velocity";
    }
};

class AccelerationColorizer : public TypedColorizer<Vector> {
public:
    AccelerationColorizer(const Interval range) {
        palette = Factory::getPalette(ColorizerId::ACCELERATION, range);
    }

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
            cached = copyable(storage.getD2t<Vector>(QuantityId::POSITION));
            values = cached;
        } else {
            values = storage.getD2t<Vector>(QuantityId::POSITION);
        }
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addD2t(QuantityId::POSITION, values[idx]);
    }

    virtual std::string name() const override {
        return "Acceleration";
    }
};

/// Shows direction of particle movement in color
class DirectionColorizer : public IColorizer {
private:
    Palette palette;
    Vector axis;
    Vector dir1, dir2;

    ArrayView<const Vector> values;
    Array<Vector> cached;

public:
    DirectionColorizer(const Vector& axis)
        : palette(Factory::getPalette(ColorizerId::MOVEMENT_DIRECTION, Interval(0._f, 2._f * PI)))
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

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
            cached = copyable(storage.getDt<Vector>(QuantityId::POSITION));
            values = cached;
        } else {
            values = storage.getDt<Vector>(QuantityId::POSITION);
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
        return Particle(idx).addDt(QuantityId::POSITION, values[idx]);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Direction";
    }
};

/// \brief Shows particle velocities with subtracted corotating component
class CorotatingVelocityColorizer : public IColorizer {
private:
    Palette palette;
    ArrayView<const Vector> r;
    ArrayView<const Vector> v;

    Vector omegaMax;

    struct {
        Array<Vector> r;
        Array<Vector> v;
    } cached;

public:
    CorotatingVelocityColorizer(const Interval range) {
        palette = Factory::getPalette(ColorizerId::VELOCITY, range);
    }

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
            cached.r = copyable(storage.getValue<Vector>(QuantityId::POSITION));
            cached.v = copyable(storage.getDt<Vector>(QuantityId::POSITION));
            r = cached.r;
            v = cached.v;
        } else {
            r = storage.getValue<Vector>(QuantityId::POSITION);
            v = storage.getDt<Vector>(QuantityId::POSITION);
        }
        Float rSqrMax = 0._f;
        for (Size i = 0; i < r.size(); ++i) {
            const Float rSqr = getSqrLength(r[i]);
            if (rSqr > rSqrMax) {
                rSqrMax = rSqr;
                const Vector perp = cross(r[i], v[i]);
                const Vector perpNorm = getNormalized(perp);
                const Vector rPerp = r[i] - dot(perpNorm, r[i]) * perpNorm;
                omegaMax = perp / getSqrLength(rPerp);
            }
        }
    }

    virtual bool isInitialized() const override {
        return !v.empty();
    }

    virtual Color eval(const Size idx) const override {
        ASSERT(!v.empty() && !r.empty());
        return palette(getLength(v[idx] - cross(omegaMax, r[idx])));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, v[idx] - cross(omegaMax, r[idx]));
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Corot. velocity";
    }
};

/// \note This does not have anything in common with QuantityId::DISPLACEMENT; it only depends on particle
/// positions and have nothing to do with stresses.
class DisplacementColorizer : public IColorizer {};

class DensityPerturbationColorizer : public IColorizer {
private:
    Palette palette;
    ArrayView<const Float> rho;
    Array<Float> cached;
    Array<Float> rho0;

public:
    DensityPerturbationColorizer(const Interval range)
        : palette(Factory::getPalette(ColorizerId::DENSITY_PERTURBATION, range)) {}

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
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

class YieldReductionColorizer : public TypedColorizer<Float> {
public:
    explicit YieldReductionColorizer()
        : TypedColorizer<Float>(QuantityId::STRESS_REDUCING,
              ColorizerId::YIELD_REDUCTION,
              Interval(0._f, 1._f)) {}

    virtual Color eval(const Size idx) const override {
        ASSERT(values);
        ASSERT(values[idx] >= 0._f && values[idx] <= 1._f);
        return palette(1._f - values[idx]);
    }

    virtual std::string name() const override {
        return "Yield reduction";
    }
};

class RadiusColorizer : public TypedColorizer<Vector> {
public:
    explicit RadiusColorizer(const Interval range) {
        TODO("finish");
        palette = Factory::getPalette(ColorizerId::RADIUS, range);
    }

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (source == ColorizerSource::CACHE_ARRAYS) {
            cached = copyable(storage.getValue<Vector>(QuantityId::POSITION));
            values = cached;
        } else {
            values = storage.getValue<Vector>(QuantityId::POSITION);
        }
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addValue(QuantityId::SMOOTHING_LENGHT, values[idx][H]);
    }

    virtual std::string name() const override {
        return "Radius";
    }
};

/// Shows boundary of bodies in the simulation.
class BoundaryColorizer : public IColorizer {
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
    BoundaryColorizer(const Detection detection, const Float threshold = 15._f)
        : detection(detection) {
        if (detection == Detection::NEIGBOUR_THRESHOLD) {
            neighbours.threshold = Size(threshold);
        } else {
            normals.threshold = threshold;
        }
    }

    virtual void initialize(const Storage& storage, const ColorizerSource source) override {
        if (detection == Detection::NORMAL_BASED) {
            if (source == ColorizerSource::CACHE_ARRAYS) {
                normals.cached = copyable(storage.getValue<Vector>(QuantityId::SURFACE_NORMAL));
                normals.values = normals.cached;
            } else {
                normals.values = storage.getValue<Vector>(QuantityId::SURFACE_NORMAL);
            }
        } else {
            if (source == ColorizerSource::CACHE_ARRAYS) {
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

class IdColorizer : public IColorizer {
private:
    /// \todo possibly move elsewhere
    static uint64_t getHash(const Size value) {
        // https://stackoverflow.com/questions/8317508/hash-function-for-a-string
        constexpr int A = 54059;
        constexpr int B = 76963;
        constexpr int FIRST = 37;

        uint64_t hash = FIRST;
        uint8_t* ptr = (uint8_t*)&value;
        for (uint i = 0; i < sizeof(uint64_t); ++i) {
            hash = (hash * A) ^ (*ptr++ * B);
        }
        return hash;
    }

public:
    virtual void initialize(const Storage& UNUSED(storage), const ColorizerSource UNUSED(source)) override {
        // no need to cache anything
    }

    virtual bool isInitialized() const override {
        return true;
    }

    virtual Color eval(const Size idx) const override {
        uint64_t hash = getHash(idx);
        const uint8_t r = (hash & 0x00000000FFFF);
        const uint8_t g = (hash & 0x0000FFFF0000) >> 16;
        const uint8_t b = (hash & 0xFFFF00000000) >> 32;
        return Color(r / 255.f, g / 255.f, b / 255.f);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual std::string name() const override {
        return "ID";
    }
};

NAMESPACE_SPH_END
