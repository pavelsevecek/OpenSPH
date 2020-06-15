#pragma once

/// \file Colorizer.h
/// \brief Object converting quantity values of particles into colors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "gravity/AggregateSolver.h"
#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "gui/objects/Texture.h"
#include "gui/renderers/Spectrum.h"
#include "objects/containers/ArrayRef.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/utility/Dynamic.h"
#include "post/Analysis.h"
#include "quantities/IMaterial.h"
#include "quantities/Particle.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

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
    virtual Rgba evalColor(const Size idx) const = 0;

    /// \brief Returns the scalar representation of the colorized quantity for idx-th particle.
    ///
    /// If there is no reasonable scalar representation (boundary particles, for example), returns NOTHING
    virtual Optional<float> evalScalar(const Size UNUSED(idx)) const {
        return NOTHING;
    }

    /// \brief Returns the vector representation of the colorized quantity for idx-th particle.
    ///
    /// If there is no reasonable vector representation (which is true for any non-vector quantity) or the
    /// function is not defined, return NOTHING.
    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const {
        return NOTHING;
    }

    /// \brief Returns the original value of the displayed quantity.
    ///
    /// If no such value exists, returns NOTHING.
    virtual Optional<Particle> getParticle(const Size idx) const = 0;

    /// \brief Returns recommended palette for drawing this colorizer.
    ///
    /// In case there is no palette, returns NOTHING.
    virtual Optional<Palette> getPalette() const = 0;

    /// \brief Modifies the palette used by ths colorizer.
    virtual void setPalette(const Palette& newPalette) = 0;

    /// \brief Returns the name of the colorizer.
    ///
    /// This is used when showing the colorizer in the window and as filename suffix.
    virtual std::string name() const = 0;
};

/// \todo
/// Types
/// Scalar -> scalar
/// Vector -> size, x, y, z
/// Tensor -> trace, 2nd inv, xx, yy, zz, xy, xz, yz, largest eigen, smallest eigen

namespace Detail {
/// Helper function returning a scalar representation of given quantity.
///
/// This value is later converted to color, using provided palette.
template <typename Type>
INLINE float getColorizerValue(const Type& value) {
    ASSERT(isReal(value));
    return float(value);
}
template <>
INLINE float getColorizerValue(const Vector& value) {
    const Float result = getLength(value);
    ASSERT(isReal(result), value);
    return float(result);
}
template <>
INLINE float getColorizerValue(const TracelessTensor& value) {
    return float(sqrt(ddot(value, value)));
}
template <>
INLINE float getColorizerValue(const SymmetricTensor& value) {
    return float(sqrt(ddot(value, value)));
}

/// Helper function returning vector representation of given quantity.
///
/// Only meaningful result is returned for vector quantity, other quantities simply return zero vector.
/// The function is useful to avoid specializing colorizers for different types.
template <typename Type>
INLINE Optional<Vector> getColorizerVector(const Type& UNUSED(value)) {
    return NOTHING;
}
template <>
INLINE Optional<Vector> getColorizerVector(const Vector& value) {
    return value;
}
} // namespace Detail

/// \brief Special colorizers that do not directly correspond to quantities.
///
/// Must have strictly negative values. Function taking \ref ColorizerId as an argument also acceps \ref
/// QuantityId casted to \ref ColorizerId, interpreting as \ref TypedColorizer with given quantity ID.
enum class ColorizerId {
    VELOCITY = -1,             ///< Particle velocities
    ACCELERATION = -2,         ///< Acceleration of particles
    MOVEMENT_DIRECTION = -3,   ///< Projected direction of motion
    COROTATING_VELOCITY = -4,  ///< Velocities with a respect to the rotating body
    DISPLACEMENT = -5,         ///< Difference between current positions and initial position
    DENSITY_PERTURBATION = -6, ///< Relative difference of density and initial density (rho/rho0 - 1)
    SUMMED_DENSITY = -7,       ///< Density computed from particle masses by direct summation of neighbours
    TOTAL_STRESS = -8,         ///< Total stress (sigma = S - pI)
    TOTAL_ENERGY = -9,         ///< Sum of kinetic and internal energy for given particle
    TEMPERATURE = -10,         ///< Temperature, computed from internal energy
    YIELD_REDUCTION = -11,     ///< Reduction of stress tensor due to yielding (1 - f_vonMises)
    DAMAGE_ACTIVATION = -12,   ///< Ratio of the stress and the activation strain
    RADIUS = -13,              ///< Radii/smoothing lenghts of particles
    DEPTH = -14,               ///< Z-depth of particles
    UVW = -15,                 ///< Shows UV mapping, u-coordinate in red and v-coordinate in blur
    BOUNDARY = -16,            ///< Shows boundary particles
    PARTICLE_ID = -17,         ///< Each particle drawn with different color
    COMPONENT_ID = -18,        ///< Color assigned to each component (group of connected particles)
    BOUND_COMPONENT_ID = -19,  ///< Color assigned to each group of gravitationally bound particles
    AGGREGATE_ID = -20,        ///< Color assigned to each aggregate
    FLAG = -21,                ///< Particles of different bodies are colored differently
    MATERIAL_ID = -22,         ///< Particles with different materials are colored differently
    BEAUTY = -23,              ///< Attempts to show the real-world look
    MARKER = -24, ///< Simple colorizer assigning given color to all particles, creating particle "mask".
};

/// \brief Default colorizer simply converting quantity value to color using defined palette.
///
/// Vector and tensor quantities are converted to floats using suitable norm.
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

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getValue<Type>(id), ref);
    }

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(this->evalScalar(idx).value());
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        ASSERT(this->isInitialized());
        return Detail::getColorizerValue(values[idx]);
    }

    virtual Optional<Vector> evalVector(const Size idx) const override {
        return Detail::getColorizerVector(values[idx]);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(id, values[idx], idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return getMetadata(id).quantityName;
    }
};

/// \brief Displays the magnitudes of particle velocities.
class VelocityColorizer : public TypedColorizer<Vector> {
public:
    explicit VelocityColorizer(Palette palette)
        : TypedColorizer<Vector>(QuantityId::POSITION, std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual Optional<Vector> evalVector(const Size idx) const override {
        return values[idx];
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, values[idx]);
    }

    virtual std::string name() const override {
        return "Velocity";
    }
};

/// \brief Displays the magnitudes of accelerations.
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

/// \brief Shows direction of particle movement in color.
class DirectionColorizer : public IColorizer {
private:
    Palette palette;
    Vector axis;
    Vector dir1, dir2;

    ArrayRef<const Vector> values;

public:
    DirectionColorizer(const Vector& axis, const Palette& palette);

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Optional<float> evalScalar(const Size idx) const override;

    virtual Rgba evalColor(const Size idx) const override {
        return palette(this->evalScalar(idx).value());
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        // return velocity of the particle
        return Particle(idx).addDt(QuantityId::POSITION, values[idx]);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
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

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override {
        return !v.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        ASSERT(!v.empty() && !r.empty());
        return palette(float(getLength(this->getCorotatingVelocity(idx))));
    }

    virtual Optional<Vector> evalVector(const Size idx) const override {
        return this->getCorotatingVelocity(idx);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, this->getCorotatingVelocity(idx));
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Corot. velocity";
    }

private:
    Vector getCorotatingVelocity(const Size idx) const {
        const BodyMetadata& body = data[matIds[idx]];
        return v[idx] - cross(body.omega, r[idx] - body.center);
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

    virtual Rgba evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        return palette(float(rho[idx] / rho0[idx] - 1._f));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::DENSITY, rho[idx] / rho0[idx] - 1.f, idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
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

    LutKernel<3> kernel;

public:
    SummedDensityColorizer(const RunSettings& settings, Palette palette);

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override {
        return !m.empty();
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        return sum(idx);
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(sum(idx));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::DENSITY, Float(sum(idx)), idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Summed Density";
    }

private:
    float sum(const Size idx) const;
};

class StressColorizer : public IColorizer {
    Palette palette;
    ArrayRef<const Float> p;
    ArrayRef<const TracelessTensor> s;

public:
    explicit StressColorizer(Palette palette)
        : palette(std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        s = makeArrayRef(storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS), ref);
        p = makeArrayRef(storage.getValue<Float>(QuantityId::PRESSURE), ref);
    }

    virtual bool isInitialized() const override {
        return !s.empty() && !p.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(this->evalScalar(idx).value());
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        ASSERT(this->isInitialized());
        SymmetricTensor sigma = SymmetricTensor(s[idx]) - p[idx] * SymmetricTensor::identity();
        // StaticArray<Float, 3> eigens = findEigenvalues(sigma);
        // return max(abs(eigens[0]), abs(eigens[1]), abs(eigens[2]));
        return float(sqrt(ddot(sigma, sigma)));
    }

    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        SymmetricTensor sigma = SymmetricTensor(s[idx]) - p[idx] * SymmetricTensor::identity();
        return Particle(QuantityId::DEVIATORIC_STRESS, sigma, idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Total stress";
    }
};

class EnergyColorizer : public IColorizer {
    Palette palette;
    ArrayRef<const Float> u;
    ArrayRef<const Vector> v;

public:
    explicit EnergyColorizer(Palette palette)
        : palette(std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        u = makeArrayRef(storage.getValue<Float>(QuantityId::ENERGY), ref);
        v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual bool isInitialized() const override {
        return !u.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(this->evalScalar(idx).value());
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        ASSERT(this->isInitialized());
        return float(u[idx] + 0.5_f * getSqrLength(v[idx]));
    }

    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        const Float value = evalScalar(idx).value();
        return Particle(QuantityId::ENERGY, value, idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Total energy";
    }
};

class TemperatureColorizer : public TypedColorizer<Float> {
    Float cp;

public:
    explicit TemperatureColorizer()
        : TypedColorizer<Float>(QuantityId::ENERGY, getEmissionPalette(Interval(500, 10000))) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        TypedColorizer<Float>::initialize(storage, ref);
        cp = storage.getMaterial(0)->getParam<Float>(BodySettingsId::HEAT_CAPACITY);
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        ASSERT(this->isInitialized());
        return float(this->values[idx] / cp);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::TEMPERATURE, values[idx] / cp, idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Temperature";
    }
};


class YieldReductionColorizer : public TypedColorizer<Float> {
public:
    explicit YieldReductionColorizer(Palette palette)
        : TypedColorizer<Float>(QuantityId::STRESS_REDUCING, std::move(palette)) {}

    virtual Rgba evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        ASSERT(values[idx] >= 0._f && values[idx] <= 1._f);
        return palette(float(1._f - values[idx]));
    }

    virtual std::string name() const override {
        return "Yield reduction";
    }
};

class DamageActivationColorizer : public IColorizer {
private:
    Palette palette;
    Array<float> ratio;

public:
    explicit DamageActivationColorizer(Palette palette)
        : palette(std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum UNUSED(ref)) override {
        ArrayView<const TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
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
                ratio[i] = float(strain / eps_min[i]);
            }
        }
    }

    virtual bool isInitialized() const override {
        return !ratio.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(ratio[idx]);
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Damage activation ratio";
    }
};

class BeautyColorizer : public IColorizer {
private:
    ArrayRef<const Float> u;
    Palette palette;

    const float u_red = 3.e5f;
    const float u_yellow = 5.e6f;
    const float u_glow = 0.5f * u_red;

public:
    BeautyColorizer() {
        palette = Palette({ { 0.1f * u_red, Rgba(0.5f, 0.5f, 0.5) },
                              { u_glow, Rgba(0.5f, 0.5f, 0.5f) },
                              { u_red, Rgba(0.8f, 0.f, 0.f) },
                              { u_yellow, Rgba(1.f, 1.f, 0.6f) } },
            PaletteScale::LOGARITHMIC);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        u = makeArrayRef(storage.getValue<Float>(QuantityId::ENERGY), ref);
    }

    virtual bool isInitialized() const override {
        return !u.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        return palette(float(u[idx]));
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        return float(max(0._f, (u[idx] - u_glow) / u_red));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addValue(QuantityId::ENERGY, u[idx]);
    }

    virtual Optional<Palette> getPalette() const override {
        return palette;
    }

    virtual void setPalette(const Palette& newPalette) override {
        palette = newPalette;
    }

    virtual std::string name() const override {
        return "Beauty";
    }
};


class RadiusColorizer : public TypedColorizer<Vector> {
public:
    explicit RadiusColorizer(Palette palette)
        : TypedColorizer<Vector>(QuantityId::SMOOTHING_LENGTH, std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    }

    virtual Rgba evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        return palette(float(values[idx][H]));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addValue(QuantityId::SMOOTHING_LENGTH, values[idx][H]);
    }

    virtual std::string name() const override {
        return "Radius";
    }
};

class DepthColorizer : public IColorizer {
private:
    Vector cameraPos;
    Float mult = 1._f;
    ArrayRef<const Vector> positions;

public:
    explicit DepthColorizer(const Vector& cameraPos, const Vector& cameraTarget)
        : cameraPos(cameraPos) {
        mult = 1._f / getLength(cameraPos - cameraTarget);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        positions = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    }

    virtual bool isInitialized() const override {
        return !positions.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return Rgba(this->evalScalar(idx).value());
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        return float(mult * getLength(positions[idx] - cameraPos));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx);
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual void setPalette(const Palette& UNUSED(newPalette)) override {}

    virtual std::string name() const override {
        return "Depth";
    }
};


class UvwColorizer : public IColorizer {
private:
    ArrayRef<const Vector> uvws;

public:
    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        uvws = makeArrayRef(storage.getValue<Vector>(QuantityId::UVW), ref);
    }

    virtual bool isInitialized() const override {
        return !uvws.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        ASSERT(this->isInitialized());
        return Rgba(float(uvws[idx][X]), 0.f, float(uvws[idx][Y]));
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual void setPalette(const Palette& UNUSED(newPalette)) override {}

    virtual std::string name() const override {
        return "Uvws";
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

    virtual Rgba evalColor(const Size idx) const override {
        if (isBoundary(idx)) {
            return Rgba::red();
        } else {
            return Rgba::gray();
        }
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        // doesn't really make sense to assign some value to boundary
        return NOTHING;
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual void setPalette(const Palette& UNUSED(newPalette)) override {}

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

class MarkerColorizer : public IColorizer {
private:
    Rgba color;

public:
    explicit MarkerColorizer(const Rgba& color)
        : color(color) {}

    virtual void initialize(const Storage& UNUSED(storage), const RefEnum UNUSED(ref)) override {}

    virtual bool isInitialized() const override {
        return true;
    }

    virtual Rgba evalColor(const Size UNUSED(idx)) const override {
        return color;
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual void setPalette(const Palette& UNUSED(newPalette)) override {}

    virtual std::string name() const override {
        return "Marker";
    }
};

/// \todo possibly move elsewhere
static uint64_t getHash(const Size value, const Size seed) {
    // https://stackoverflow.com/questions/8317508/hash-function-for-a-string
    constexpr int A = 54059;
    constexpr int B = 76963;
    constexpr int FIRST = 37;

    uint64_t hash = FIRST + seed;
    uint8_t* ptr = (uint8_t*)&value;
    for (uint i = 0; i < sizeof(uint64_t); ++i) {
        hash = (hash * A) ^ (*ptr++ * B);
    }
    return hash;
}

static Rgba getRandomizedColor(const Size idx, const Size seed = 0) {
    const uint64_t hash = getHash(idx, seed);
    const uint8_t r = (hash & 0x00000000FFFF);
    const uint8_t g = (hash & 0x0000FFFF0000) >> 16;
    const uint8_t b = (hash & 0xFFFF00000000) >> 32;
    return Rgba(r / 255.f, g / 255.f, b / 255.f);
}

template <typename TDerived>
class IdColorizerTemplate : public IColorizer {
private:
    Rgba backgroundColor;
    Size seed = 1;

public:
    explicit IdColorizerTemplate(const GuiSettings& gui) {
        backgroundColor = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    }

    void setSeed(const Size newSeed) {
        seed = newSeed;
    }

    virtual Rgba evalColor(const Size idx) const override {
        const Optional<Size> id = static_cast<const TDerived*>(this)->evalId(idx);
        if (!id) {
            return Rgba::gray();
        }
        const Rgba color = getRandomizedColor(id.value(), seed);
        /*if (backgroundColor.intensity() < 0.5f) {
            // dark background, brighten the particle color
            return color.brighten(0.2f);
        } else {
            // light background, darken the particle color
            return color.darken(0.2f);
        }*/
        return color;
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        Particle particle(idx);
        const Optional<Size> id = static_cast<const TDerived*>(this)->evalId(idx);
        if (id) {
            particle.addValue(QuantityId::FLAG, id.value());
        }
        return particle;
    }

    virtual Optional<Palette> getPalette() const override {
        return NOTHING;
    }

    virtual void setPalette(const Palette& UNUSED(newPalette)) override {}
};

class ParticleIdColorizer : public IdColorizerTemplate<ParticleIdColorizer> {
public:
    using IdColorizerTemplate<ParticleIdColorizer>::IdColorizerTemplate;

    INLINE Optional<Size> evalId(const Size idx) const {
        return idx;
    }

    virtual void initialize(const Storage& UNUSED(storage), const RefEnum UNUSED(ref)) override {
        // no need to cache anything
    }

    virtual bool isInitialized() const override {
        return true;
    }

    virtual std::string name() const override {
        return "Particle ID";
    }
};

class ComponentIdColorizer : public IdColorizerTemplate<ComponentIdColorizer> {
private:
    Flags<Post::ComponentFlag> connectivity;

    Array<Size> components;

    ArrayRef<const Float> m;
    ArrayRef<const Vector> r, v;

    Optional<Size> highlightIdx;

    /// \todo hacked optimization to avoid rebuilding the colorizer if only highlight idx changed. Should be
    /// done more generally and with lower memory footprint
    struct {
        Array<Vector> r;
    } cached;

public:
    explicit ComponentIdColorizer(const GuiSettings& gui,
        const Flags<Post::ComponentFlag> connectivity,
        const Optional<Size> highlightIdx = NOTHING)
        : IdColorizerTemplate<ComponentIdColorizer>(gui)
        , connectivity(connectivity)
        , highlightIdx(highlightIdx) {}

    void setHighlightIdx(const Optional<Size> newHighlightIdx) {
        if (newHighlightIdx) {
            highlightIdx = min(newHighlightIdx.value(), components.size() - 1);
        } else {
            highlightIdx = NOTHING;
        }
    }

    Optional<Size> getHighlightIdx() const {
        return highlightIdx;
    }

    void setConnectivity(const Flags<Post::ComponentFlag> newConnectivity) {
        connectivity = newConnectivity;
        cached.r.clear();
    }

    Flags<Post::ComponentFlag> getConnectivity() const {
        return connectivity;
    }

    INLINE Optional<Size> evalId(const Size idx) const {
        return components[idx];
    }

    virtual Rgba evalColor(const Size idx) const override {
        if (highlightIdx) {
            if (highlightIdx.value() == components[idx]) {
                return Rgba(1.f, 0.65f, 0.f);
            } else {
                return Rgba::gray(0.3f);
            }
        } else {
            return IdColorizerTemplate<ComponentIdColorizer>::evalColor(idx);
        }
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        Particle particle(idx);
        const Optional<Size> id = this->evalId(idx);
        particle.addValue(QuantityId::FLAG, id.value());

        Array<Size> indices;
        for (Size i = 0; i < r.size(); ++i) {
            if (components[i] == id.value()) {
                indices.push(i);
            }
        }
        if (indices.size() > 1) {
            const Vector omega = Post::getAngularFrequency(m, r, v, indices);
            particle.addValue(QuantityId::ANGULAR_FREQUENCY, getLength(omega));
        }
        return particle;
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        const Array<Vector>& current = storage.getValue<Vector>(QuantityId::POSITION);
        if (current == cached.r) {
            // optimization, very poorly done
            return;
        }

        m = makeArrayRef(storage.getValue<Float>(QuantityId::MASS), ref);
        r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
        v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);

        cached.r = current.clone();

        Post::findComponents(storage, 2._f, connectivity, components);
    }

    virtual bool isInitialized() const override {
        return !components.empty();
    }

    virtual std::string name() const override {
        if (connectivity.has(Post::ComponentFlag::ESCAPE_VELOCITY)) {
            return "Bound component ID";
        } else if (connectivity.has(Post::ComponentFlag::SEPARATE_BY_FLAG)) {
            return "Component ID (flag)";
        } else {
            return "Component ID";
        }
    }
};

class AggregateIdColorizer : public IdColorizerTemplate<AggregateIdColorizer> {
private:
    ArrayView<const Size> ids;

public:
    using IdColorizerTemplate<AggregateIdColorizer>::IdColorizerTemplate;

    INLINE Optional<Size> evalId(const Size idx) const {
        if (ids[idx] != Size(-1)) {
            return ids[idx];
        } else {
            return NOTHING;
        }
    }

    virtual void initialize(const Storage& storage, const RefEnum UNUSED(ref)) override {
        ids = storage.getValue<Size>(QuantityId::AGGREGATE_ID);
    }

    virtual bool isInitialized() const override {
        return ids != nullptr;
    }

    virtual std::string name() const override {
        return "Aggregate ID";
    }
};


class IndexColorizer : public IdColorizerTemplate<IndexColorizer> {
private:
    QuantityId id;
    ArrayRef<const Size> idxs;

public:
    IndexColorizer(const QuantityId id, const GuiSettings& gui)
        : IdColorizerTemplate<IndexColorizer>(gui)
        , id(id) {}

    INLINE Optional<Size> evalId(const Size idx) const {
        return idxs[idx];
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        idxs = makeArrayRef(storage.getValue<Size>(id), ref);
    }

    virtual bool isInitialized() const override {
        return !idxs.empty();
    }

    virtual std::string name() const override {
        return getMetadata(id).quantityName;
    }
};

NAMESPACE_SPH_END
