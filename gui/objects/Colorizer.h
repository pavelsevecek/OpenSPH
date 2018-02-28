#pragma once

/// \file Colorizer.h
/// \brief Object converting quantity values of particles into colors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018


#include "gui/Factory.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "objects/containers/ArrayRef.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/utility/Dynamic.h"
#include "quantities/IMaterial.h"
#include "quantities/Particle.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

class Particle;

/// \brief Interface for objects assigning colors to particles.
///
/// Used to add a layer of abstraction between quantity values and displayed colors, allowing to visualize
/// various information that isn't directly stored as quantity, like relative values of quantitiees, angular
/// dependence of velocities, etc. Usually though, one wants to display raw quantity values, which can be
/// accomplished by \ref TypedColorizer.
class IColorizer : public Polymorphic {
public:
    /// \brief Initialize the colorizer before by getting necessary quantities from storage.
    ///
    /// Must be called before \ref evalColor is called, every time step as ArrayViews taken from storage might
    /// be invalidated.
    /// \param storage Particle storage containing source data to be drawn.
    /// \param ref Specifies how the object refereneces the data required for evaluation; either the buffers
    ///            are copied and stored in the colorizer, or only references to the the storage are kept.
    virtual void initialize(const Storage& storage, const RefEnum ref) = 0;

    /// \brief Checks if the colorizer has been initialized.
    virtual bool isInitialized() const = 0;

    /// \brief Returns the color of idx-th particle.
    virtual Color evalColor(const Size idx) const = 0;

    /// \brief Returns the vector representation of the colorized quantity for idx-th particle.
    ///
    /// If there is no reasonable vector representation (which is true for any non-vector quantity) or the
    /// function is not defined, return zero vector.
    virtual Vector evalVector(const Size UNUSED(idx)) const {
        return Vector(0._f);
    }

    /// \brief Returns the original value of the displayed quantity.
    ///
    /// If no such value exists, returns NOTHING.
    virtual Optional<Particle> getParticle(const Size idx) const = 0;

    /// \brief Returns recommended palette for drawing this colorizer.
    ///
    /// In case there is no palette, returns NOTHING.
    virtual Optional<Palette> getPalette() const = 0;

    /// \brief Returns the name of the colorizer.
    ///
    /// This is used when showing the colorizer in the window and as filename suffix.
    virtual std::string name() const = 0;
};


namespace Detail {
    /// Helper function returning a scalar representation of given quantity.
    ///
    /// This value is later converted to color, using provided palette.
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

    /// Helper function returning vector representation of given quantity.
    ///
    /// Only meaningful result is returned for vector quantity, other quantities simply return zero vector.
    /// The function is useful to avoid specializing colorizers for different types.
    template <typename Type>
    INLINE Vector getColorizerVector(const Type& UNUSED(value)) {
        return Vector(0._f);
    }
    template <>
    INLINE Vector getColorizerVector(const Vector& value) {
        return value;
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
    SUMMED_DENSITY = -7,       ///< Density computed from particle masses by direct summation of neighbours
    YIELD_REDUCTION = -8,      ///< Reduction of stress tensor due to yielding (1 - f_vonMises)
    DAMAGE_ACTIVATION = -9,    ///< Ratio of the stress and the activation strain
    RADIUS = -10,              ///< Radii/smoothing lenghts of particles
    BOUNDARY = -11,            ///< Shows boundary particles
    ID = -12,                  ///< Each particle drawn with different color
};

/// Default colorizer simply converting quantity value to color using defined palette. Vector and tensor
/// quantities are converted to floats using suitable norm.
template <typename Type>
class TypedColorizer : public IColorizer {
protected:
    QuantityId id;
    Palette palette;
    ArrayRef<const Type> values;

public:
    TypedColorizer(const QuantityId id, Palette palette)
        : id(id)
        , palette(std::move(palette)) {}

    TypedColorizer(const QuantityId id, const ColorizerId colorizerId, const Interval range)
        : id(id)
        , palette(Factory::getPalette(colorizerId, range)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getValue<Type>(id), ref);
    }

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Color evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        return palette(Detail::getColorizerValue(values[idx]));
    }

    virtual Vector evalVector(const Size idx) const {
        return Detail::getColorizerVector(values[idx]);
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
    explicit VelocityColorizer(Palette palette)
        : TypedColorizer<Vector>(QuantityId::POSITION, std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual Vector evalVector(const Size idx) const override {
        return values[idx];
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
    explicit AccelerationColorizer(Palette palette)
        : TypedColorizer<Vector>(QuantityId::POSITION, std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getD2t<Vector>(QuantityId::POSITION), ref);
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

    ArrayRef<const Vector> values;

public:
    explicit DirectionColorizer(const Vector& axis)
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

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Color evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
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
    ArrayRef<const Vector> r;
    ArrayRef<const Vector> v;
    ArrayRef<const Size> matIds;

    struct BodyMetadata {
        Vector center;
        Vector omega;
    };

    Array<BodyMetadata> data;

public:
    explicit CorotatingVelocityColorizer(Palette palette)
        : palette(std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
        v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
        matIds = makeArrayRef(storage.getValue<Size>(QuantityId::MATERIAL_ID), ref);

        /// \todo this works correctly only for bodies with no initial velocity
        data.resize(storage.getMaterialCnt());
        for (Size i = 0; i < data.size(); ++i) {
            MaterialView mat = storage.getMaterial(i);
            data[i].center = mat->getParam<Vector>(BodySettingsId::BODY_CENTER);
            data[i].omega = mat->getParam<Vector>(BodySettingsId::BODY_ANGULAR_VELOCITY);
        }
    }

    virtual bool isInitialized() const override {
        return !v.empty();
    }

    virtual Color evalColor(const Size idx) const override {
        ASSERT(!v.empty() && !r.empty());
        return palette(getLength(this->getCorotatingVelocity(idx)));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, this->getCorotatingVelocity(idx));
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Corot. velocity";
    }

private:
    Vector getCorotatingVelocity(const Size idx) const {
        const BodyMetadata& body = data[matIds[idx]];
        return v[idx] - cross(body.omega, r[idx]);
    }
};

class DensityPerturbationColorizer : public IColorizer {
private:
    Palette palette;
    ArrayRef<const Float> rho;
    Array<Float> rho0;

public:
    explicit DensityPerturbationColorizer(Palette palette)
        : palette(std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        rho = makeArrayRef(storage.getValue<Float>(QuantityId::DENSITY), ref);

        rho0.resize(rho.size());
        for (Size i = 0; i < rho.size(); ++i) {
            rho0[i] = storage.getMaterialOfParticle(i)->getParam<Float>(BodySettingsId::DENSITY);
        }
    }

    virtual bool isInitialized() const override {
        return !rho.empty();
    }

    virtual Color evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
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

class SummedDensityColorizer : public IColorizer {
private:
    Palette palette;
    ArrayRef<const Float> m;
    ArrayRef<const Vector> r;

    AutoPtr<IBasicFinder> finder;
    mutable Array<NeighbourRecord> neighs;

    LutKernel<3> kernel;

public:
    explicit SummedDensityColorizer(const RunSettings& settings, Palette palette)
        : palette(std::move(palette)) {
        finder = Factory::getFinder(settings);
        kernel = Factory::getKernel<3>(settings);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        m = makeArrayRef(storage.getValue<Float>(QuantityId::MASS), ref);
        r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);

        finder->build(r);
    }

    virtual bool isInitialized() const override {
        return !m.empty();
    }

    virtual Color evalColor(const Size idx) const override {
        return palette(sum(idx));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::DENSITY, sum(idx), idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Summed Density";
    }

private:
    Float sum(const Size idx) const {
        finder->findAll(idx, r[idx][H] * kernel.radius(), neighs);
        Float rho = 0._f;
        for (const auto& n : neighs) {
            rho += m[n.index] * kernel.value(r[idx] - r[n.index], r[idx][H]);
        }
        return rho;
    }
};

class YieldReductionColorizer : public TypedColorizer<Float> {
public:
    explicit YieldReductionColorizer(Palette palette)
        : TypedColorizer<Float>(QuantityId::STRESS_REDUCING, std::move(palette)) {}

    virtual Color evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        ASSERT(values[idx] >= 0._f && values[idx] <= 1._f);
        return palette(1._f - values[idx]);
    }

    virtual std::string name() const override {
        return "Yield reduction";
    }
};

class DamageActivationColorizer : public IColorizer {
private:
    Palette palette;
    Array<Float> ratio;

public:
    explicit DamageActivationColorizer(Palette palette)
        : palette(std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum UNUSED(ref)) override {
        ArrayView<const TracelessTensor> s =
            storage.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
        ArrayView<const Float> eps_min = storage.getValue<Float>(QuantityId::EPS_MIN);
        ArrayView<const Float> damage = storage.getValue<Float>(QuantityId::DAMAGE);

        ratio.resize(p.size());
        /// \todo taken from ScalarGradyKippDamage, could be deduplicated
        for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
            MaterialView mat = storage.getMaterial(matId);
            const Float young = mat->getParam<Float>(BodySettingsId::YOUNG_MODULUS);

            /// \todo parallelize
            for (Size i : mat.sequence()) {
                const SymmetricTensor sigma = SymmetricTensor(s[i]) - p[i] * SymmetricTensor::identity();
                Float sig1, sig2, sig3;
                tie(sig1, sig2, sig3) = findEigenvalues(sigma);
                const Float sigMax = max(sig1, sig2, sig3);
                const Float young_red = max((1._f - pow<3>(damage[i])) * young, 1.e-20_f);
                const Float strain = sigMax / young_red;
                ratio[i] = strain / eps_min[i];
            }
        }
    }

    virtual bool isInitialized() const override {
        return !ratio.empty();
    }

    virtual Color evalColor(const Size idx) const override {
        return palette(ratio[idx]);
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual std::string name() const override {
        return "Damage activation ratio";
    }
};

class RadiusColorizer : public TypedColorizer<Vector> {
public:
    explicit RadiusColorizer(Palette palette)
        : TypedColorizer<Vector>(QuantityId::SMOOTHING_LENGHT, std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    }

    virtual Color evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        return palette(values[idx][H]);
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
        ArrayRef<const Vector> values;
        Float threshold;
    } normals;

    struct {
        ArrayRef<const Size> values;
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

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        if (detection == Detection::NORMAL_BASED) {
            normals.values = makeArrayRef(storage.getValue<Vector>(QuantityId::SURFACE_NORMAL), ref);
        } else {
            neighbours.values = makeArrayRef(storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT), ref);
        }
    }

    virtual bool isInitialized() const override {
        return (detection == Detection::NORMAL_BASED && !normals.values.empty()) ||
               (detection == Detection::NEIGBOUR_THRESHOLD && !neighbours.values.empty());
    }

    virtual Color evalColor(const Size idx) const override {
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
            ASSERT(!neighbours.values.empty());
            return neighbours.values[idx] < neighbours.threshold;
        case Detection::NORMAL_BASED:
            ASSERT(!normals.values.empty());
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
    virtual void initialize(const Storage& UNUSED(storage), const RefEnum UNUSED(ref)) override {
        // no need to cache anything
    }

    virtual bool isInitialized() const override {
        return true;
    }

    virtual Color evalColor(const Size idx) const override {
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
