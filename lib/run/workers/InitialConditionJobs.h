#pragma once

#include "gravity/Galaxy.h"
#include "objects/containers/Grid.h"
#include "run/workers/MaterialJobs.h"

NAMESPACE_SPH_BEGIN

/// \brief Creates a single monolithic body
class MonolithicBodyIc : public IParticleJob, public MaterialProvider {
protected:
    struct {
        bool shape = false;
        bool material = false;
    } slotUsage;

public:
    MonolithicBodyIc(const std::string& name, const BodySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "create monolithic body";
    }

    virtual UnorderedMap<std::string, JobType> requires() const override;

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
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
    explicit DifferentiatedBodyIc(const std::string& name);

    virtual std::string className() const override {
        return "create differentiated body";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        UnorderedMap<std::string, JobType> slots = {
            { "base shape", JobType::GEOMETRY },
            { "base material", JobType::MATERIAL },
        };
        for (int i = 0; i < layerCnt; ++i) {
            slots.insert("shape " + std::to_string(i + 1), JobType::GEOMETRY);
            slots.insert("material " + std::to_string(i + 1), JobType::MATERIAL);
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
    Float mass = Constants::M_sun;
    Float radius = Constants::R_sun;
    int flag = 0;

public:
    explicit SingleParticleIc(const std::string& name)
        : IParticleJob(name) {}

    virtual std::string className() const override {
        return "create single particle";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class ImpactorIc : public MonolithicBodyIc {
public:
    ImpactorIc(const std::string& name, const BodySettings& overrides = EMPTY_SETTINGS)
        : MonolithicBodyIc(name, overrides) {}

    virtual std::string className() const override {
        return "create impactor";
    }

    virtual UnorderedMap<std::string, JobType> requires() const override {
        UnorderedMap<std::string, JobType> map = MonolithicBodyIc::requires();
        map.insert("target", JobType::PARTICLES);
        return map;
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
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

class EquilibriumIc : public IParticleJob {
private:
    EnumWrapper solver;
    int boundaryThreshold;

public:
    EquilibriumIc(const std::string& name);

    virtual std::string className() const override {
        return "set equilibrium energy";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
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
    ModifyQuantityIc(const std::string& name);

    virtual std::string className() const override {
        return "modify quantity";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class NoiseQuantityIc : public IParticleJob {
private:
    EnumWrapper id;

    Float mean = 1.f;
    Float magnitude = 1.f;

public:
    NoiseQuantityIc(const std::string& name);

    virtual std::string className() const override {
        return "Perlin noise";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
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
    DOMAIN_RADIUS,
    RADIAL_PROFILE,
    HEIGHT_SCALE,
    POWER_LAW_INTERVAL,
    POWER_LAW_EXPONENT,
    VELOCITY_MULTIPLIER,
    VELOCITY_DISPERSION,
};

using NBodySettings = Settings<NBodySettingsId>;

class NBodyIc : public IParticleJob {
private:
    NBodySettings settings;

public:
    NBodyIc(const std::string& name, const NBodySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "N-body ICs";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return { { "shape", JobType::GEOMETRY } };
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
    IsothermalSphereIc(const std::string& name);

    virtual std::string className() const override {
        return "isothermal sphere ICs";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class GalaxyIc : public IParticleJob {
private:
    GalaxySettings settings;

public:
    GalaxyIc(const std::string& name, const GalaxySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "galaxy ICs";
    }

    virtual UnorderedMap<std::string, JobType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
