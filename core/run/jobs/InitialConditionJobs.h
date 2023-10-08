#pragma once

#include "objects/containers/Grid.h"
#include "run/jobs/MaterialJobs.h"
#include "sph/initial/Galaxy.h"

NAMESPACE_SPH_BEGIN

/// \brief Creates a single monolithic body
class MonolithicBodyIc : public IParticleJob, public MaterialProvider {
protected:
    struct {
        bool shape = false;
        bool material = false;
    } slotUsage;

public:
    explicit MonolithicBodyIc(const String& name, const BodySettings& overrides = EMPTY_SETTINGS);

    virtual String className() const override {
        return "create monolithic body";
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override;

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "shape", JobType::GEOMETRY }, { "material", JobType::MATERIAL } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

protected:
    virtual void addParticleCategory(VirtualSettings& settings);
};


/// \brief Creates a single differentiated body.
class DifferentiatedBodyIc : public IParticleJob {
private:
    BodySettings mainBody;
    int layerCnt = 1;

public:
    explicit DifferentiatedBodyIc(const String& name);

    virtual String className() const override {
        return "create differentiated body";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        UnorderedMap<String, ExtJobType> slots = {
            { "base shape", JobType::GEOMETRY },
            { "base material", JobType::MATERIAL },
        };
        for (int i = 0; i < layerCnt; ++i) {
            slots.insert("shape " + toString(i + 1), JobType::GEOMETRY);
            slots.insert("material " + toString(i + 1), JobType::MATERIAL);
        }
        return slots;
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class SingleParticleIc : public IParticleJob {
private:
    Vector r0 = Vector(0._f);
    Vector v0 = Vector(0._f);
    Float mass = Constants::M_earth;
    Float radius = Constants::R_earth;
    EnumWrapper interaction;
    Float springConstant = 0.004_f;
    Float epsilon = 0.5_f;

    bool visible = true;
    Float albedo = 1._f;
    Path texture;

public:
    explicit SingleParticleIc(const String& name);

    virtual String className() const override {
        return "create single particle";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class ImpactorIc : public MonolithicBodyIc {
public:
    explicit ImpactorIc(const String& name, const BodySettings& overrides = EMPTY_SETTINGS)
        : MonolithicBodyIc(name, overrides) {}

    virtual String className() const override {
        return "create impactor";
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override {
        UnorderedMap<String, ExtJobType> map = MonolithicBodyIc::requires();
        map.insert("target", JobType::PARTICLES);
        return map;
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "target", JobType::PARTICLES },
            { "shape", JobType::GEOMETRY },
            { "material", JobType::MATERIAL },
        };
    }

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

private:
    virtual void addParticleCategory(VirtualSettings& settings) override;
};

class EquilibriumEnergyIc : public IParticleJob {
private:
    EnumWrapper solver;
    int boundaryThreshold;

public:
    explicit EquilibriumEnergyIc(const String& name);

    virtual String className() const override {
        return "set equilibrium energy";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class EquilibriumDensityIc : public IParticleJob {
private:
    EnumWrapper temperatureProfile;

public:
    explicit EquilibriumDensityIc(const String& name);

    virtual String className() const override {
        return "set equilibrium density";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class KeplerianVelocityIc : public IParticleJob {
public:
    explicit KeplerianVelocityIc(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "set Keplerian velocity";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "orbiting", JobType::PARTICLES },
            { "gravity source", JobType::PARTICLES },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class ModifyQuantityIc : public IParticleJob {
private:
    EnumWrapper id;
    EnumWrapper mode;

    Float centralValue = 1.f;
    Float radialGrad = 0.f;

    ExtraEntry curve;

public:
    explicit ModifyQuantityIc(const String& name);

    virtual String className() const override {
        return "modify quantity";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class NoiseQuantityIc : public IParticleJob {
private:
    Float magnitude = 1._f;
    Vector gridDims = Vector(8, 8, 8);
    int seed = 1234;

public:
    explicit NoiseQuantityIc(const String& name);

    virtual String className() const override {
        return "Perlin noise";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

private:
    template <Size Dims, typename TSetter>
    void randomize(IRunCallbacks& callbacks, ArrayView<const Vector> r, const TSetter& setter) const;

    Float perlin(const Grid<Vector>& gradients, const Vector& v) const;

    Float dotGradient(const Grid<Vector>& gradients, const Indices& i, const Vector& v) const;
};

enum class NBodySettingsId {
    PARTICLE_COUNT,
    TOTAL_MASS,
    RADIAL_PROFILE,
    HEIGHT_SCALE,
    POWER_LAW_INTERVAL,
    POWER_LAW_EXPONENT,
    MIN_SEPARATION,
    VELOCITY_MULTIPLIER,
    VELOCITY_DISPERSION,
};

using NBodySettings = Settings<NBodySettingsId>;

class NBodyIc : public IParticleJob {
private:
    NBodySettings settings;

public:
    explicit NBodyIc(const String& name, const NBodySettings& overrides = EMPTY_SETTINGS);

    virtual String className() const override {
        return "N-body ICs";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "domain", JobType::GEOMETRY },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class PolytropeIc : public IParticleJob {
private:
    int particleCnt = 10000;
    EnumWrapper distId = EnumWrapper(DistributionEnum::PARAMETRIZED_SPIRALING);

    Float radius = 1.e7_f;
    Float rho_min = 10._f;
    Float n = 1._f;
    Float eta = 1.3_f;

    Path texture;

public:
    explicit PolytropeIc(const String& name);

    virtual String className() const override {
        return "polytrope ICs";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "material", JobType::MATERIAL } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class IsothermalSphereIc : public IParticleJob {
private:
    Float radius = 1.e6_f;
    Float centralDensity = 1000._f;
    Float centralEnergy = 1000._f;
    Float gamma = 4._f / 3._f;
    int particleCnt = 10000;

public:
    explicit IsothermalSphereIc(const String& name);

    virtual String className() const override {
        return "isothermal sphere ICs";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class GalaxyIc : public IParticleJob {
private:
    GalaxySettings settings;

public:
    explicit GalaxyIc(const String& name, const GalaxySettings& overrides = EMPTY_SETTINGS);

    virtual String className() const override {
        return "galaxy ICs";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
