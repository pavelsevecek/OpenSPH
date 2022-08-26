#pragma once

#include "run/Job.h"

NAMESPACE_SPH_BEGIN

class JoinParticlesJob : public IParticleJob {
private:
    Vector offset = Vector(0._f);
    Vector velocity = Vector(0._f);
    bool moveToCom = false;
    bool uniqueFlags = true;

public:
    explicit JoinParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "join";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "particles A", JobType::PARTICLES },
            { "particles B", JobType::PARTICLES },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class OrbitParticlesJob : public IParticleJob {
private:
    Float a = 1.e6_f;
    Float e = 0._f;
    Float v = 0._f;

public:
    explicit OrbitParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "orbit";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "particles A", JobType::PARTICLES },
            { "particles B", JobType::PARTICLES },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class MultiJoinParticlesJob : public IParticleJob {
private:
    int slotCnt = 3;
    bool moveToCom = false;
    bool uniqueFlags = true;

public:
    explicit MultiJoinParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "multi join";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        UnorderedMap<String, ExtJobType> map;
        for (int i = 0; i < slotCnt; ++i) {
            map.insert("particles " + toString(i + 1), JobType::PARTICLES);
        }
        return map;
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class TransformParticlesJob : public IParticleJob {
private:
    struct {
        Vector offset = Vector(0._f);
        Vector angles = Vector(0._f);
    } positions;

    struct {
        Vector offset = Vector(0._f);
        Float mult = 1.;
    } velocities;

    Vector spin = Vector(0._f);

public:
    explicit TransformParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "transform";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class ScatterJob : public IParticleJob {
private:
    int number = 5;

public:
    explicit ScatterJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "scatter";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES }, { "domain", JobType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};


class CenterParticlesJob : public IParticleJob {
private:
    bool centerPositions = true;
    bool centerVelocities = false;

public:
    explicit CenterParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "center";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class ChangeMaterialSubset {
    ALL,
    MATERIAL_ID,
    INSIDE_DOMAIN,
};

class ChangeMaterialJob : public IParticleJob {
private:
    EnumWrapper type = EnumWrapper(ChangeMaterialSubset::ALL);
    int matId = 0;

public:
    explicit ChangeMaterialJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "change material";
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override {
        UnorderedMap<String, ExtJobType> map{
            { "particles", JobType::PARTICLES },
            { "material", JobType::MATERIAL },
        };
        if (ChangeMaterialSubset(type) == ChangeMaterialSubset::INSIDE_DOMAIN) {
            map.insert("domain", JobType::GEOMETRY);
        }
        return map;
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES },
            { "material", JobType::MATERIAL },
            { "domain", JobType::GEOMETRY } };
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

class CollisionGeometrySetupJob : public IParticleJob {
private:
    CollisionGeometrySettings geometry;

public:
    explicit CollisionGeometrySetupJob(const String& name,
        const CollisionGeometrySettings& overrides = EMPTY_SETTINGS);

    virtual String className() const override {
        return "collision setup";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "target", JobType::PARTICLES }, { "impactor", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SmoothedToSolidHandoffJob : public IParticleJob {
private:
    EnumWrapper type;

    /// \brief Conversion factor between smoothing length and particle radius.
    ///
    /// Used only for HandoffRadius::SMOOTHING_LENGTH.
    Float radiusMultiplier = 0.333_f;

public:
    explicit SmoothedToSolidHandoffJob(const String& name);

    virtual String className() const override {
        return "smoothed-to-solid handoff";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class MergeOverlappingParticlesJob : public IParticleJob {
private:
    Float surfacenessThreshold = 0.5_f;
    int minComponentSize = 100;
    int iterationCnt = 3;

public:
    explicit MergeOverlappingParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "merge overlapping particles";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class ExtractComponentJob : public IParticleJob {
private:
    int componentIdx = 0;
    Float factor = 1.5_f;
    bool center = false;

public:
    explicit ExtractComponentJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "extract component";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};


class RemoveParticlesJob : public IParticleJob {
private:
    bool removeDamaged = true;
    bool removeExpanded = false;
    Float damageLimit = 1._f;
    Float energyLimit = 1.e6_f;


public:
    explicit RemoveParticlesJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "remove particles";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class ConnectivityEnum {
    OVERLAP,
    ESCAPE_VELOCITY,
};

class MergeComponentsJob : public IParticleJob {
private:
    Float factor = 1.5_f;
    EnumWrapper connectivity = EnumWrapper(ConnectivityEnum::OVERLAP);

public:
    explicit MergeComponentsJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "merge components";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class ExtractParticlesInDomainJob : public IParticleJob {
private:
    bool center = false;

public:
    explicit ExtractParticlesInDomainJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "extract particles in domain";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES }, { "domain", JobType::GEOMETRY } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class EmplaceComponentsAsFlagsJob : public IParticleJob {
private:
    Float factor = 1.5_f;

public:
    explicit EmplaceComponentsAsFlagsJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "emplace components";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "fragments", JobType::PARTICLES },
            { "original", JobType::PARTICLES },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class SubsampleJob : public IParticleJob {
private:
    Float fraction = 0.5_f;
    bool adjustMass = true;
    bool adjustRadii = true;

public:
    explicit SubsampleJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "subsampler";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

enum class CompareMode {
    PARTICLE_WISE,
    LARGE_PARTICLES_ONLY,
};

class CompareJob : public IParticleJob {
    EnumWrapper mode = EnumWrapper(CompareMode::PARTICLE_WISE);
    Float eps = 1.e-4_f;
    Float fraction = 0.2_f;
    Float maxDeviation = 0.5_f;

public:
    explicit CompareJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "compare";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "test particles", JobType::PARTICLES },
            { "reference particles", JobType::PARTICLES },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

NAMESPACE_SPH_END
