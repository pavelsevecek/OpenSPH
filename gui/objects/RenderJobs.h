#pragma once

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/CameraJobs.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/IRenderer.h"

NAMESPACE_SPH_BEGIN

struct RenderParams;
class JobNode;
class Palette;

enum class RenderColorizerId {
    VELOCITY = int(ColorizerId::VELOCITY),
    ENERGY = int(QuantityId::ENERGY),
    DENSITY = int(QuantityId::DENSITY),
    DAMAGE = int(QuantityId::DAMAGE),
    GRAVITY = 666,
    BEAUTY = int(ColorizerId::BEAUTY),
};

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

    virtual void remove(ArrayView<const Size> UNUSED(idxs)) override {}
};

class AnimationJob : public IImageJob {
private:
    GuiSettings gui;
    EnumWrapper colorizerId;
    ExtraEntry paletteEntry;
    bool addSurfaceGravity = true;
    Path directory;
    String fileMask = "img_%d.png";

    EnumWrapper animationType;

    bool transparentBackground = false;
    int extraFrames = 0;

    struct {
        Path firstFile = Path("out_0000.ssf");
        EnumWrapper units;
    } sequence;

public:
    explicit AnimationJob(const String& name);

    virtual String className() const override {
        return "render animation";
    }

    virtual UnorderedMap<String, ExtJobType> getSlots() const override {
        return {
            { "particles", JobType::PARTICLES },
            { "camera", GuiJobType::CAMERA },
        };
    }

    virtual VirtualSettings getSettings() override;

    virtual void evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) override;

    AutoPtr<IRenderPreview> getRenderPreview(const RunSettings& global) const;

    // needed for interactive rendering
    AutoPtr<IRenderer> getRenderer(const RunSettings& global) const;
    Palette getPalette() const;
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
