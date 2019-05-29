#pragma once

#include "gui/Settings.h"
#include "run/Worker.h"

NAMESPACE_SPH_BEGIN

enum class ColorizerFlag {
    VELOCITY = 1 << 0,
    ENERGY = 1 << 1,
    BOUND_COMPONENT_ID = 1 << 2,
    BEAUTY = 1 << 3,
};

enum class AnimationType {
    SINGLE_FRAME,
    ORBIT,
    FILE_SEQUENCE,
};

class AnimationWorker : public IParticleWorker {
private:
    GuiSettings gui;
    Flags<ColorizerFlag> colorizers = ColorizerFlag::VELOCITY;

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
    AnimationWorker(const std::string& name);

    virtual std::string className() const override {
        return "render animation";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        return { { "particles", WorkerType::PARTICLES } };
    }

    virtual UnorderedMap<std::string, WorkerType> requires() const override {
        if (AnimationType(animationType) == AnimationType::FILE_SEQUENCE) {
            return {};
        } else {
            return { { "particles", WorkerType::PARTICLES } };
        }
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;
};

NAMESPACE_SPH_END
