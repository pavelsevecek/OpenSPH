#pragma once

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/CameraJobs.h"
#include "gui/renderers/IRenderer.h"

NAMESPACE_SPH_BEGIN

class IColorizer;
struct RenderParams;
class JobNode;
class Palette;

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

    virtual void update(AutoPtr<IColorizer>&& colorizer) = 0;

    virtual void update(AutoPtr<IRenderer>&& renderer) = 0;

    virtual void update(Palette&& palette) = 0;

    virtual void cancel() = 0;
};

class IImageJob : public IJob {
private:
    SharedPtr<Bitmap<Rgba>> result;

protected:
    GuiSettings gui;

public:
    explicit IImageJob(const std::string& name)
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

class AnimationJob : public IImageJob {
private:
    GuiSettings gui;
    EnumWrapper colorizerId;
    bool addSurfaceGravity = true;
    Path directory;
    std::string fileMask = "img_%d.png";

    EnumWrapper animationType;

    bool transparentBackground = false;
    int extraFrames = 0;

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

    virtual UnorderedMap<std::string, ExtJobType> requires() const override;

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

    AutoPtr<IRenderPreview> getRenderPreview(const RunSettings& global) const;

    // needed for interactive rendering
    AutoPtr<IRenderer> getRenderer(const RunSettings& global) const;
    AutoPtr<IColorizer> getColorizer(const RunSettings& global) const;
    RenderParams getRenderParams() const;

private:
    RenderParams getRenderParams(const GuiSettings& gui) const;
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
    VdbJob(const std::string& name)
        : IParticleJob(name) {}

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
