#pragma once

#include "gravity/Galaxy.h"
#include "run/workers/MaterialWorkers.h"

NAMESPACE_SPH_BEGIN

/// \brief Creates a single monolithic body
class MonolithicBodyIc : public IParticleWorker, public MaterialProvider {
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

    virtual UnorderedMap<std::string, WorkerType> requires() const override;

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "shape", WorkerType::GEOMETRY }, { "material", WorkerType::MATERIAL } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

protected:
    virtual void addParticleCategory(VirtualSettings& settings);
};


/// \brief Creates a single differentiated body.
class DifferentiatedBodyIc : public IParticleWorker {
private:
    BodySettings mainBody;
    int layerCnt = 1;

public:
    explicit DifferentiatedBodyIc(const std::string& name);

    virtual std::string className() const override {
        return "create differentiated body";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        UnorderedMap<std::string, WorkerType> slots = {
            { "base shape", WorkerType::GEOMETRY },
            { "base material", WorkerType::MATERIAL },
        };
        for (int i = 0; i < layerCnt; ++i) {
            slots.insert("shape " + std::to_string(i + 1), WorkerType::GEOMETRY);
            slots.insert("material " + std::to_string(i + 1), WorkerType::MATERIAL);
        }
        return slots;
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

    virtual UnorderedMap<std::string, WorkerType> requires() const override {
        UnorderedMap<std::string, WorkerType> map = MonolithicBodyIc::requires();
        map.insert("target", WorkerType::PARTICLES);
        return map;
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {
            { "target", WorkerType::PARTICLES },
            { "shape", WorkerType::GEOMETRY },
            { "material", WorkerType::MATERIAL },
        };
    }

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

private:
    virtual void addParticleCategory(VirtualSettings& settings) override;
};

class EquilibriumIc : public IParticleWorker {
public:
    EquilibriumIc(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "set equilibrium energy";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class ModifyQuantityIc : public IParticleWorker {
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

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
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

class NBodyIc : public IParticleWorker {
private:
    NBodySettings settings;

public:
    NBodyIc(const std::string& name, const NBodySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "N-body ICs";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "shape", WorkerType::GEOMETRY } };
    }


    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class GalaxyIc : public IParticleWorker {
private:
    GalaxySettings settings;

public:
    GalaxyIc(const std::string& name, const GalaxySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "galaxy ICs";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {};
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
