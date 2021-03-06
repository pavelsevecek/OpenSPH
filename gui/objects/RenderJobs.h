#pragma once

#include "gui/Settings.h"
#include "gui/objects/CameraJobs.h"

NAMESPACE_SPH_BEGIN

enum class ColorizerFlag {
    VELOCITY = 1 << 0,
    ENERGY = 1 << 1,
    BOUND_COMPONENT_ID = 1 << 2,
    MASS = 1 << 3,
    BEAUTY = 1 << 4,
    GRAVITY = 1 << 5,
    DAMAGE = 1 << 6,
};

enum class AnimationType {
    SINGLE_FRAME,
    ORBIT,
    FILE_SEQUENCE,
};

class AnimationJob : public INullJob {
private:
    GuiSettings gui;
    Flags<ColorizerFlag> colorizers = ColorizerFlag::VELOCITY;
    bool addSurfaceGravity = true;

    EnumWrapper animationType = EnumWrapper(AnimationType::SINGLE_FRAME);

    bool transparentBackground = false;

    struct {
        Float step = 10._f * DEG_TO_RAD;
        Float finalAngle = 360._f * DEG_TO_RAD;
    } orbit;

    struct {
        Path firstFile = Path("out_0000.ssf");
    } sequence;

public:
    explicit AnimationJob(const std::string& name);

    virtual std::string className() const override {
        return "render animation";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return {
            { "particles", JobType::PARTICLES },
            { "camera", GuiJobType::CAMERA },
        };
    }

    virtual UnorderedMap<std::string, ExtJobType>
    requires() const override {
        if (AnimationType(animationType) == AnimationType::FILE_SEQUENCE) {
            return { { "camera", GuiJobType::CAMERA } };
        } else {
            return this->getSlots();
        }
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

class VdbJob : public INullJob {
private:
    Vector gridStart = Vector(-1.e5_f);
    Vector gridEnd = Vector(1.e5_f);
    int dimPower = 10;
    Float surfaceLevel = 0.13_f;

    struct {
        bool enabled = false;
        Path firstFile = Path("out_0000.ssf");
    } sequence;

    Path path = Path("grid.vdb");

public:
    VdbJob(const std::string& name)
        : INullJob(name) {}

    virtual std::string className() const override {
        return "save VDB grid";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual UnorderedMap<std::string, ExtJobType>
    requires() const override {
        if (sequence.enabled) {
            return {};
        } else {
            return { { "particles", JobType::PARTICLES } };
        }
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

private:
    void generate(Storage& storage, const RunSettings& global, const Path& outputPath);
};

NAMESPACE_SPH_END
