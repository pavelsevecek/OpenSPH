#pragma once

#include "gui/objects/CameraJobs.h"
#include "gui/objects/Shader.h"

NAMESPACE_SPH_BEGIN

class ShaderJob : public IJob {
private:
    SharedPtr<IShader> result;

    EnumWrapper colorizerId;
    Float lower = 0._f;
    Float upper = 1.e6_f;
    EnumWrapper scale;

    ExtraEntry palette;
    ExtraEntry curve;

public:
    explicit ShaderJob(const String& name);

    virtual String className() const override {
        return "shader";
    }

    virtual ExtJobType provides() const override {
        return GuiJobType::SHADER;
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override {
        return {};
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {};
    }

    virtual JobContext getResult() const override final {
        return result;
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

NAMESPACE_SPH_END
