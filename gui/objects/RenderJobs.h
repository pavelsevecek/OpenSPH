#pragma once

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/CameraJobs.h"
#include "gui/renderers/IRenderer.h"

NAMESPACE_SPH_BEGIN

class IColorizer;
struct RenderParams;
class JobNode;
class ColorLut;

enum class RenderColorizerId;

enum class AnimationType {
    SINGLE_FRAME = 0,
    FILE_SEQUENCE = 2,
};

class IRenderPreview : public Polymorphic {
public:
    virtual void render(const Pixel resolution, IRenderOutput& output) = 0;

    virtual void update(RenderParams&& params) = 0;

    virtual void update(AutoPtr<ICamera>&& newCamera) = 0;

    //    virtual void update(AutoPtr<IColorizer>&& colorizer) = 0;

    virtual void update(AutoPtr<IRenderer>&& renderer) = 0;

    virtual void update(ColorLut&& palette) = 0;

    virtual void cancel() = 0;
};

class IImageJob : public IJob {
private:
    SharedPtr<Bitmap<Rgba>> result;

protected:
    GuiSettings gui;

public:
    explicit IImageJob(const String& name)
        : IJob(name) {}

    virtual ExtJobType provides() const override final {
        return GuiJobType::IMAGE;
    }

    virtual JobContext getResult() const override final {
        return result;
    }
};

struct AnimationFrame : public IStorageUserData {
    Bitmap<Rgba> bitmap;
    Array<IRenderOutput::Label> labels;
};

struct SequenceParams {
    Path firstFile = Path("out_0000.ssf");
    int extraFrames = 0;
};

class IRenderJob : public IImageJob {
protected:
    GuiSettings gui;
    EnumWrapper colorizerId;
    bool transparentBackground = false;
    bool addSurfaceGravity = true;

    Path directory;
    String fileMask = "img_%d.png";

    EnumWrapper animationType;

    SequenceParams sequence;

public:
    explicit IRenderJob(const String& name)
        : IImageJob(name) {}

    virtual void evaluate(const RunSettings& global, IRunCallbacks& callbacks) override final;

    AutoPtr<IRenderPreview> getRenderPreview(const RunSettings& global) const;

    // needed for interactive rendering
    virtual AutoPtr<IRenderer> getRenderer(const RunSettings& global) const = 0;
    RenderParams getRenderParams() const;

private:
    RenderParams getRenderParams(const GuiSettings& gui) const;
};

class ParticleRenderJob : public IRenderJob {
public:
    explicit ParticleRenderJob(const String& name);

    virtual String className() const override {
        return "particle renderer";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "particles", JobType::PARTICLES },
            { "camera", GuiJobType::CAMERA },
        };
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override;

    virtual VirtualSettings getSettings() override;

private:
    AutoPtr<IColorizer> getColorizer(const RunSettings& global) const;
    virtual AutoPtr<IRenderer> getRenderer(const RunSettings& global) const override;
};

enum class ShaderFlag {
    SURFACENESS = 1 << 0,
    EMISSION = 1 << 1,
    SCATTERING = 1 << 2,
    ABSORPTION = 1 << 3,
};

class RaytracerJob : public IRenderJob {
private:
    Flags<ShaderFlag> shaderFlags = ShaderFlag::SURFACENESS;

public:
    explicit RaytracerJob(const String& name);

    virtual String className() const override {
        return "raytracer";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "particles", JobType::PARTICLES },
            { "camera", GuiJobType::CAMERA },
            { "surfaceness", GuiJobType::SHADER },
            { "emission", GuiJobType::SHADER },
            { "scattering", GuiJobType::SHADER },
            { "absorption", GuiJobType::SHADER },
        };
    }

    virtual UnorderedMap<String, ExtJobType>
    requires() const override;

    virtual VirtualSettings getSettings() override;

private:
    virtual AutoPtr<IRenderer> getRenderer(const RunSettings& global) const override;
};

class VdbJob : public IParticleJob {
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
    explicit VdbJob(const String& name)
        : IParticleJob(name) {}

    virtual String className() const override {
        return "save VDB grid";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return { { "particles", JobType::PARTICLES } };
    }

    virtual UnorderedMap<String, ExtJobType>
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
