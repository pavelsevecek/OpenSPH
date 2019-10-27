#pragma once

#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

class CachedParticlesWorker : public IParticleWorker {
private:
    ParticleData cached;
    bool doSwitch = false;
    bool useCached = false;

public:
    CachedParticlesWorker(const std::string& name, const Storage& storage = {});

    virtual std::string className() const override {
        return "cache";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        if (useCached) {
            return {};
        } else {
            return { { "particles", WorkerType::PARTICLES } };
        }
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class JoinParticlesWorker : public IParticleWorker {
private:
    Vector offset = Vector(0._f);
    Vector velocity = Vector(0._f);
    bool moveToCom = false;
    bool uniqueFlags = false;

public:
    JoinParticlesWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "join";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles A", WorkerType::PARTICLES }, { "particles B", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};


class TransformParticlesWorker : public IParticleWorker {
private:
    struct {
        Vector offset = Vector(0._f);
        Vector angles = Vector(0._f);
    } positions;

    struct {
        Vector offset = Vector(0._f);
        Float mult = 1.;
    } velocities;

public:
    TransformParticlesWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "transform";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class ChangeMaterialSubset {
    ALL,
    MATERIAL_ID,
    INSIDE_DOMAIN,
};

class ChangeMaterialWorker : public IParticleWorker {
private:
    EnumWrapper type = EnumWrapper(ChangeMaterialSubset::ALL);
    int matId = 0;

public:
    ChangeMaterialWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "change material";
    }

    virtual UnorderedMap<std::string, WorkerType> requires() const override {
        UnorderedMap<std::string, WorkerType> map{
            { "particles", WorkerType::PARTICLES },
            { "material", WorkerType::MATERIAL },
        };
        if (ChangeMaterialSubset(type) == ChangeMaterialSubset::INSIDE_DOMAIN) {
            map.insert("domain", WorkerType::GEOMETRY);
        }
        return map;
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES },
            { "material", WorkerType::MATERIAL },
            { "domain", WorkerType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};


enum class CollisionGeometrySettingsId {
    /// Impact angle in degrees, i.e. angle between velocity vector and normal at the impact point.
    IMPACT_ANGLE,

    /// Impact speed in m/s
    IMPACT_SPEED,

    /// Initial distance of the impactor from the impact point. This value is in units of smoothing length h.
    /// Should not be lower than kernel.radius() * eta.
    IMPACTOR_OFFSET,

    /// If true, derivatives in impactor will be computed with lower precision. This significantly improves
    /// the performance of the code. The option is intended mainly for cratering impacts and should be always
    /// false when simulating collision of bodies of comparable sizes.
    IMPACTOR_OPTIMIZE,

    /// If true, positions and velocities of particles are modified so that center of mass is at origin and
    /// has zero velocity.
    CENTER_OF_MASS_FRAME,
};

using CollisionGeometrySettings = Settings<CollisionGeometrySettingsId>;

class CollisionGeometrySetup : public IParticleWorker {
private:
    CollisionGeometrySettings geometry;

public:
    CollisionGeometrySetup(const std::string& name,
        const CollisionGeometrySettings& overrides = EMPTY_SETTINGS);

    virtual std::string className() const override {
        return "collision setup";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "target", WorkerType::PARTICLES }, { "impactor", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class SmoothedToSolidHandoff : public IParticleWorker {
public:
    explicit SmoothedToSolidHandoff(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "smoothed-to-solid handoff";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class ExtractComponentWorker : public IParticleWorker {
private:
    int componentIdx = 0;
    Float factor = 1.5_f;
    bool center = false;

public:
    explicit ExtractComponentWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "extract component";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class ConnectivityEnum {
    OVERLAP,
    ESCAPE_VELOCITY,
};

class MergeComponentsWorker : public IParticleWorker {
private:
    Float factor = 1.5_f;
    EnumWrapper connectivity = EnumWrapper(ConnectivityEnum::OVERLAP);

public:
    explicit MergeComponentsWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "merge components";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class ExtractParticlesInDomainWorker : public IParticleWorker {
private:
    bool center = false;

public:
    explicit ExtractParticlesInDomainWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "extract particles in domain";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES }, { "domain", WorkerType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class EmplaceComponentsAsFlagsWorker : public IParticleWorker {
private:
    Float factor = 1.5_f;

public:
    explicit EmplaceComponentsAsFlagsWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "emplace components";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return {
            { "fragments", WorkerType::PARTICLES },
            { "original", WorkerType::PARTICLES },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SubsampleWorker : public IParticleWorker {
private:
    Float fraction = 0.5_f;

public:
    explicit SubsampleWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "subsampler";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class AnalysisWorker : public IParticleWorker {
private:
    Path outputPath = Path("report.txt");

public:
    explicit AnalysisWorker(const std::string& name)
        : IParticleWorker(name) {}

    virtual std::string className() const override {
        return "analysis";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
