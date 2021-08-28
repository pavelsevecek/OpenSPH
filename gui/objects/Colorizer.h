#pragma once

/// \file Colorizer.h
/// \brief Object converting quantity values of particles into colors.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gravity/AggregateSolver.h"
#include "gui/Settings.h"
#include "gui/objects/Palette.h"
#include "gui/objects/Point.h"
#include "gui/objects/Texture.h"
#include "gui/renderers/Spectrum.h"
#include "objects/containers/ArrayRef.h"
#include "objects/finders/NeighborFinder.h"
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
    /// \brief Checks if the storage constains all data necessary to initialize the colorizer.
    virtual bool hasData(const Storage& storage) const = 0;

    /// \brief Initialize the colorizer before by getting necessary quantities from storage.
    ///
    /// Can only be called if \ref hasData returns true. Must be called before \ref evalColor is called, every
    /// time step as ArrayViews taken from storage might be invalidated.
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

    /// \brief Returns recommended LUT for drawing this colorizer.
    ///
    /// In case there is no LUT, returns NOTHING.
    virtual Optional<ColorLut> getColorLut() const = 0;

    /// \brief Modifies the color LUT used by the colorizer.
    virtual void setColorLut(const ColorLut& newLut) = 0;

    /// \brief Returns the name of the colorizer.
    ///
    /// This is used when showing the colorizer in the window and as filename suffix.
    virtual String name() const = 0;
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
    SPH_ASSERT(isReal(value));
    return float(value);
}
template <>
INLINE float getColorizerValue(const Vector& value) {
    const Float result = getLength(value);
    SPH_ASSERT(isReal(result), value);
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

/// \brief Default colorizer simply converting quantity value to color using defined palette.
///
/// Vector and tensor quantities are converted to floats using suitable norm.
template <typename Type>
class TypedColorizer : public IColorizer {
protected:
    QuantityId id;
    ColorLut lut;
    ArrayRef<const Type> values;

public:
    TypedColorizer(const QuantityId id, ColorLut lut)
        : id(id)
        , lut(std::move(lut)) {}

    virtual bool hasData(const Storage& storage) const override {
        return storage.has(id);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getValue<Type>(id), ref);
    }

    virtual bool isInitialized() const override {
        return !values.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return lut(this->evalScalar(idx).value());
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        SPH_ASSERT(this->isInitialized());
        return Detail::getColorizerValue(values[idx]);
    }

    virtual Optional<Vector> evalVector(const Size idx) const override {
        return Detail::getColorizerVector(values[idx]);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(id, values[idx], idx);
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return lut;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        lut = newPalette;
    }

    virtual String name() const override {
        return getMetadata(id).quantityName;
    }
};

inline bool hasVelocity(const Storage& storage) {
    return storage.has<Vector>(QuantityId::POSITION, OrderEnum::FIRST) ||
           storage.has<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
}

/// \brief Displays the magnitudes of particle velocities.
class VelocityColorizer : public TypedColorizer<Vector> {
public:
    explicit VelocityColorizer(ColorLut palette)
        : TypedColorizer<Vector>(QuantityId::POSITION, std::move(palette)) {}

    virtual bool hasData(const Storage& storage) const override {
        return hasVelocity(storage);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual Optional<Vector> evalVector(const Size idx) const override {
        return values[idx];
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, values[idx]);
    }

    virtual String name() const override {
        return "Velocity";
    }
};

/// \brief Displays the magnitudes of accelerations.
class AccelerationColorizer : public TypedColorizer<Vector> {
public:
    explicit AccelerationColorizer(ColorLut palette)
        : TypedColorizer<Vector>(QuantityId::POSITION, std::move(palette)) {}

    virtual bool hasData(const Storage& storage) const override {
        return storage.has<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getD2t<Vector>(QuantityId::POSITION), ref);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addD2t(QuantityId::POSITION, values[idx]);
    }

    virtual String name() const override {
        return "Acceleration";
    }
};

/// \brief Shows direction of particle movement in color.
class DirectionColorizer : public IColorizer {
private:
    ColorLut palette;
    Vector axis;
    Vector dir1, dir2;

    ArrayRef<const Vector> values;

public:
    DirectionColorizer(const Vector& axis, const ColorLut& palette);

    virtual bool hasData(const Storage& storage) const override {
        return hasVelocity(storage);
    }

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

    virtual Optional<ColorLut> getColorLut() const override {
        return palette;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        palette = newPalette;
    }

    virtual String name() const override {
        return "Direction";
    }
};

/// \brief Shows particle velocities with subtracted corotating component
class CorotatingVelocityColorizer : public IColorizer {
private:
    ColorLut palette;
    ArrayRef<const Vector> r;
    ArrayRef<const Vector> v;
    ArrayRef<const Size> matIds;

    struct BodyMetadata {
        Vector center;
        Vector omega;
    };

    Array<BodyMetadata> data;

public:
    explicit CorotatingVelocityColorizer(ColorLut palette)
        : palette(std::move(palette)) {}

    virtual bool hasData(const Storage& storage) const override {
        return hasVelocity(storage) && storage.has(QuantityId::MATERIAL_ID);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override {
        return !v.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        SPH_ASSERT(!v.empty() && !r.empty());
        return palette(float(getLength(this->getCorotatingVelocity(idx))));
    }

    virtual Optional<Vector> evalVector(const Size idx) const override {
        return this->getCorotatingVelocity(idx);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addDt(QuantityId::POSITION, this->getCorotatingVelocity(idx));
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return palette;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        palette = newPalette;
    }

    virtual String name() const override {
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
    ColorLut palette;
    ArrayRef<const Float> rho;
    Array<Float> rho0;

public:
    explicit DensityPerturbationColorizer(ColorLut palette)
        : palette(std::move(palette)) {}

    virtual bool hasData(const Storage& storage) const override {
        return storage.has(QuantityId::DENSITY);
    }

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
        SPH_ASSERT(this->isInitialized());
        return palette(float(rho[idx] / rho0[idx] - 1._f));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::DENSITY, rho[idx] / rho0[idx] - 1.f, idx);
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return palette;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        palette = newPalette;
    }

    virtual String name() const override {
        return "Delta Density";
    }
};

class SummedDensityColorizer : public IColorizer {
private:
    ColorLut palette;
    ArrayRef<const Float> m;
    ArrayRef<const Vector> r;

    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

public:
    SummedDensityColorizer(const RunSettings& settings, ColorLut palette);

    virtual bool hasData(const Storage& UNUSED(storage)) const override {
        // mass and positions must always be present
        return true;
    }

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

    virtual Optional<ColorLut> getColorLut() const override {
        return palette;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        palette = newPalette;
    }

    virtual String name() const override {
        return "Summed Density";
    }

private:
    float sum(const Size idx) const;
};

class StressColorizer : public IColorizer {
    ColorLut palette;
    ArrayRef<const Float> p;
    ArrayRef<const TracelessTensor> s;

public:
    explicit StressColorizer(ColorLut palette)
        : palette(std::move(palette)) {}

    virtual bool hasData(const Storage& storage) const override {
        return storage.has(QuantityId::DEVIATORIC_STRESS) && storage.has(QuantityId::PRESSURE);
    }

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
        SPH_ASSERT(this->isInitialized());
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

    virtual Optional<ColorLut> getColorLut() const override {
        return palette;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        palette = newPalette;
    }

    virtual String name() const override {
        return "Total stress";
    }
};

class EnergyColorizer : public IColorizer {
    ColorLut lut;
    ArrayRef<const Float> u;
    ArrayRef<const Vector> v;

public:
    explicit EnergyColorizer(ColorLut lut)
        : lut(std::move(lut)) {}

    virtual bool hasData(const Storage& storage) const override {
        return hasVelocity(storage) && storage.has(QuantityId::ENERGY);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        u = makeArrayRef(storage.getValue<Float>(QuantityId::ENERGY), ref);
        v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    }

    virtual bool isInitialized() const override {
        return !u.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return lut(this->evalScalar(idx).value());
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        SPH_ASSERT(this->isInitialized());
        return float(u[idx] + 0.5_f * getSqrLength(v[idx]));
    }

    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        const Float value = evalScalar(idx).value();
        return Particle(QuantityId::ENERGY, value, idx);
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return lut;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        lut = newPalette;
    }

    virtual String name() const override {
        return "Total energy";
    }
};

class TemperatureColorizer : public TypedColorizer<Float> {
    Float cp;

public:
    explicit TemperatureColorizer()
        : TypedColorizer<Float>(QuantityId::ENERGY,
              ColorLut(getEmissionPalette(Interval(500, 10000)),
                  Interval(500, 10000),
                  PaletteScale::LINEAR)) {}

    virtual bool hasData(const Storage& storage) const override {
        return storage.has(QuantityId::ENERGY) && storage.getMaterialCnt() > 0;
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        TypedColorizer<Float>::initialize(storage, ref);
        cp = storage.getMaterial(0)->getParam<Float>(BodySettingsId::HEAT_CAPACITY);
    }

    virtual Optional<float> evalScalar(const Size idx) const override {
        SPH_ASSERT(this->isInitialized());
        return float(this->values[idx] / cp);
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(QuantityId::TEMPERATURE, values[idx] / cp, idx);
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return lut;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        lut = newPalette;
    }

    virtual String name() const override {
        return "Temperature";
    }
};


class YieldReductionColorizer : public TypedColorizer<Float> {
public:
    explicit YieldReductionColorizer(ColorLut palette)
        : TypedColorizer<Float>(QuantityId::STRESS_REDUCING, std::move(palette)) {}

    virtual Rgba evalColor(const Size idx) const override {
        SPH_ASSERT(this->isInitialized());
        SPH_ASSERT(values[idx] >= 0._f && values[idx] <= 1._f);
        return lut(float(1._f - values[idx]));
    }

    virtual String name() const override {
        return "Yield reduction";
    }
};

class DamageActivationColorizer : public IColorizer {
private:
    ColorLut palette;
    Array<float> ratio;

public:
    explicit DamageActivationColorizer(const ColorLut& palette)
        : palette(std::move(palette)) {}

    virtual bool hasData(const Storage& storage) const override;

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override {
        return !ratio.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(ratio[idx]);
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return palette;
    }

    virtual void setColorLut(const ColorLut& newPalette) override {
        palette = newPalette;
    }

    virtual String name() const override {
        return "Damage activation ratio";
    }
};

class RadiusColorizer : public TypedColorizer<Vector> {
public:
    explicit RadiusColorizer(ColorLut palette)
        : TypedColorizer<Vector>(QuantityId::SMOOTHING_LENGTH, std::move(palette)) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        values = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    }

    virtual Rgba evalColor(const Size idx) const override {
        SPH_ASSERT(this->isInitialized());
        return lut(float(values[idx][H]));
    }

    virtual Optional<Particle> getParticle(const Size idx) const override {
        return Particle(idx).addValue(QuantityId::SMOOTHING_LENGTH, values[idx][H]);
    }

    virtual bool hasData(const Storage& UNUSED(storage)) const override {
        // radii are always present
        return true;
    }

    virtual String name() const override {
        return "Radius";
    }
};

class UvwColorizer : public IColorizer {
private:
    ArrayRef<const Vector> uvws;

public:
    virtual bool hasData(const Storage& storage) const override {
        return storage.has(QuantityId::UVW);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        uvws = makeArrayRef(storage.getValue<Vector>(QuantityId::UVW), ref);
    }

    virtual bool isInitialized() const override {
        return !uvws.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        SPH_ASSERT(this->isInitialized());
        return Rgba(float(uvws[idx][X]), 0.f, float(uvws[idx][Y]));
    }

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return NOTHING;
    }

    virtual void setColorLut(const ColorLut& UNUSED(newPalette)) override {}

    virtual String name() const override {
        return "Uvws";
    }
};

/// Shows boundary of bodies in the simulation.
class BoundaryColorizer : public IColorizer {
public:
    enum class Detection {
        /// Particles with fewer neighbors are considered boundary. Not suitable if number of neighbors is
        /// enforced by adapting smoothing length. Note that increasing the threshold adds more particles into
        /// the boundary.
        NEIGBOR_THRESHOLD,

        /// Boundary is determined by relative position vectors approximating surface normal. Has higher
        /// overhead, but does not depend sensitively on number of neighbors. Here, increasing the threshold
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
    } neighbors;

public:
    BoundaryColorizer(const Detection detection, const Float threshold = 15._f);

    virtual bool hasData(const Storage& storage) const override;

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override;

    virtual Rgba evalColor(const Size idx) const override;

    virtual Optional<Particle> getParticle(const Size UNUSED(idx)) const override {
        // doesn't really make sense to assign some value to boundary
        return NOTHING;
    }

    virtual Optional<ColorLut> getColorLut() const override {
        return NOTHING;
    }

    virtual void setColorLut(const ColorLut& UNUSED(newPalette)) override {}

    virtual String name() const override {
        return "Boundary";
    }

private:
    bool isBoundary(const Size idx) const;
};

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

    virtual Rgba evalColor(const Size idx) const override;

    virtual Optional<Particle> getParticle(const Size idx) const override;

    virtual Optional<ColorLut> getColorLut() const override {
        return NOTHING;
    }

    virtual void setColorLut(const ColorLut& UNUSED(newPalette)) override {}
};

class ParticleIdColorizer : public IdColorizerTemplate<ParticleIdColorizer> {
private:
    ArrayRef<const Size> persistentIdxs;

public:
    using IdColorizerTemplate<ParticleIdColorizer>::IdColorizerTemplate;

    INLINE Optional<Size> evalId(const Size idx) const {
        if (!persistentIdxs.empty() && idx < persistentIdxs.size()) {
            return persistentIdxs[idx];
        } else {
            return idx;
        }
    }

    virtual bool hasData(const Storage& UNUSED(storage)) const override {
        return true;
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override {
        return true;
    }

    virtual Optional<Particle> getParticle(const Size idx) const override;

    virtual String name() const override {
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
        const Optional<Size> highlightIdx = NOTHING);

    void setHighlightIdx(const Optional<Size> newHighlightIdx);

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

    virtual Rgba evalColor(const Size idx) const override;

    virtual Optional<Particle> getParticle(const Size idx) const override;

    virtual bool hasData(const Storage& storage) const override;

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual bool isInitialized() const override {
        return !components.empty();
    }

    virtual String name() const override;
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

    virtual bool hasData(const Storage& storage) const override {
        return storage.has(QuantityId::AGGREGATE_ID) && storage.getUserData() != nullptr;
    }

    virtual void initialize(const Storage& storage, const RefEnum UNUSED(ref)) override {
        ids = storage.getValue<Size>(QuantityId::AGGREGATE_ID);
    }

    virtual bool isInitialized() const override {
        return ids != nullptr;
    }

    virtual String name() const override {
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

    virtual bool hasData(const Storage& storage) const override {
        return storage.has(id);
    }

    virtual void initialize(const Storage& storage, const RefEnum ref) override {
        idxs = makeArrayRef(storage.getValue<Size>(id), ref);
    }

    virtual bool isInitialized() const override {
        return !idxs.empty();
    }

    virtual String name() const override {
        return getMetadata(id).quantityName;
    }
};

class MaterialColorizer : public IndexColorizer {
private:
    Array<String> eosNames;
    Array<String> rheoNames;

public:
    MaterialColorizer(const GuiSettings& gui)
        : IndexColorizer(QuantityId::MATERIAL_ID, gui) {}

    virtual void initialize(const Storage& storage, const RefEnum ref) override;

    virtual Optional<Particle> getParticle(const Size idx) const override;
};

NAMESPACE_SPH_END
