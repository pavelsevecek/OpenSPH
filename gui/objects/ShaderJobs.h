#pragma once

#include "gui/objects/CameraJobs.h"
#include "gui/objects/Shader.h"

NAMESPACE_SPH_BEGIN

class IShaderJob : public IJob {
protected:
    SharedPtr<IShader> result;

public:
    explicit IShaderJob(const String& name)
        : IJob(name) {}

    virtual ExtJobType provides() const override final {
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
};

class ColorEntry : public IExtraEntry {
private:
    Rgba color;

public:
    ColorEntry() = default;

    ColorEntry(const Rgba& color)
        : color(color) {}

    virtual String toString() const override {
        return format("{},{},{}", color.r(), color.g(), color.b());
    }

    virtual void fromString(const String& s) override {
        sscanf(s.toAscii(), "%f%f%f", &color.r(), &color.g(), &color.b());
    }

    virtual AutoPtr<IExtraEntry> clone() const override {
        return makeAuto<ColorEntry>(color);
    }

    Rgba getColor() const {
        return color;
    }
};

class ColorShaderJob : public IShaderJob {
private:
    ExtraEntry color;
    Float mult = 1._f;

public:
    explicit ColorShaderJob(const String& name);

    virtual String className() const override {
        return "color shader";
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

class QuantityShaderJob : public IShaderJob {
private:
    EnumWrapper colorizerId;
    Float lower = 0._f;
    Float upper = 1.e6_f;
    EnumWrapper scale;
    Float mult = 1._f;

    ExtraEntry palette;
    ExtraEntry curve;

public:
    explicit QuantityShaderJob(const String& name);

    virtual String className() const override {
        return "shader";
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override;
};

NAMESPACE_SPH_END
