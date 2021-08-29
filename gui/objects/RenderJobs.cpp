#include "gui/objects/RenderJobs.h"
#include "gravity/BarnesHut.h"
#include "gravity/Moments.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "gui/objects/Shader.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayMarcher.h"
#include "gui/windows/PaletteEditor.h"
#include "run/IRun.h"
#include "run/VirtualSettings.h"
#include "run/jobs/IoJobs.h"
#include "system/Factory.h"
#include "system/Timer.h"

#ifdef SPH_USE_VDB
#include <openvdb/openvdb.h>
#endif

NAMESPACE_SPH_BEGIN

class GravityColorizer : public TypedColorizer<Float> {
private:
    SharedPtr<IScheduler> scheduler;
    BarnesHut gravity;
    Array<Float> acc;
    Float G;
    bool addSurfaceGravity;

public:
    GravityColorizer(const SharedPtr<IScheduler>& scheduler,
        const ColorLut& palette,
        const Float G,
        const bool addSurfaceGravity)
        : TypedColorizer<Float>(QuantityId::POSITION, std::move(palette))
        , scheduler(scheduler)
        , gravity(0.8_f, MultipoleOrder::OCTUPOLE, 25, 50, G)
        , G(G)
        , addSurfaceGravity(addSurfaceGravity) {}

    virtual void initialize(const Storage& storage, const RefEnum UNUSED(ref)) override {
        acc.resize(storage.getParticleCnt());
        acc.fill(0._f);

        // gravitation acceleration from other particles
        gravity.build(*scheduler, storage);

        Array<Vector> dv(storage.getParticleCnt());
        dv.fill(Vector(0._f));
        Statistics stats;
        gravity.evalSelfGravity(*scheduler, dv, stats);
        for (Size i = 0; i < dv.size(); ++i) {
            acc[i] = getLength(dv[i]);
        }

        if (addSurfaceGravity) {
            // add surface gravity of each particle
            ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
            ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
            for (Size i = 0; i < r.size(); ++i) {
                acc[i] += G * m[i] / sqr(r[i][H]);
            }
        }
    }

    virtual bool isInitialized() const override {
        return !acc.empty();
    }

    virtual Rgba evalColor(const Size idx) const override {
        return lut(acc[idx]);
    }

    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual String name() const override {
        // needs to 'pretend' to be acceleration to work with palette accessor in IR
        return "Acceleration";
    }
};

class AnimationRenderOutput : public IRenderOutput {
private:
    IRunCallbacks& callbacks;
    IRenderer& renderer;
    Size iterationCnt;

    Timer timer;
    Size iteration = 0;

public:
    AnimationRenderOutput(IRunCallbacks& callbacks, IRenderer& renderer, const Size iterationCnt)
        : callbacks(callbacks)
        , renderer(renderer)
        , iterationCnt(iterationCnt) {}

    virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels, const bool isFinal) override {
        this->update(bitmap.clone(), std::move(labels), isFinal);
    }

    virtual void update(Bitmap<Rgba>&& bitmap, Array<Label>&& labels, const bool UNUSED(isFinal)) override {
        SharedPtr<AnimationFrame> frame = makeShared<AnimationFrame>();
        frame->bitmap = std::move(bitmap);
        frame->labels = std::move(labels);
        Storage storage;
        storage.setUserData(frame);

        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, Float(++iteration) / iterationCnt);
        stats.set(StatisticsId::WALLCLOCK_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
        callbacks.onTimeStep(storage, stats);

        if (callbacks.shouldAbortRun()) {
            renderer.cancelRender();
        }
    }
};

class RenderPreview : public IRenderPreview {
private:
    RenderParams params;
    AutoPtr<IRenderer> renderer;
    SharedPtr<ParticleData> data;
    std::atomic_bool cancelled;

    bool rendererDirty = true;

public:
    RenderPreview(RenderParams&& params, AutoPtr<IRenderer>&& renderer, const SharedPtr<ParticleData>& data)
        : params(std::move(params))
        , renderer(std::move(renderer))
        , data(data)
        , cancelled(false) {}

    virtual void render(const Pixel resolution, IRenderOutput& output) override {
        cancelled = false;

        // lazy init
        if (rendererDirty) {
            renderer->initialize(data->storage, *params.camera);
            rendererDirty = false;
        }
        if (cancelled) {
            return;
        }

        Pixel size = params.camera->getSize();
        size = correctAspectRatio(resolution, float(size.x) / float(size.y));
        params.camera->resize(size);
        Statistics dummy;
        renderer->render(params, dummy, output);
    }

    virtual void update(RenderParams&& newParams) override {
        AutoPtr<ICamera> camera = std::move(params.camera);
        params = std::move(newParams);
        params.camera = std::move(camera);
    }

    virtual void update(AutoPtr<ICamera>&& newCamera) override {
        params.camera = std::move(newCamera);
    }

    virtual void update(AutoPtr<IRenderer>&& newRenderer) override {
        renderer = std::move(newRenderer);
        rendererDirty = true;
    }

    virtual void update(ColorLut&& palette) override {
        (void)palette;
        /// \todo
        //  colorizer->setColorLut(palette);
        // renderer->setColorizer(*colorizer);
    }

    virtual void cancel() override {
        cancelled = true;
        renderer->cancelRender();
    }

private:
    Pixel correctAspectRatio(const Pixel resolution, const float aspect) const {
        const float current = float(resolution.x) / float(resolution.y);
        if (current > aspect) {
            return Pixel(resolution.x * aspect / current, resolution.y);
        } else {
            return Pixel(resolution.x, resolution.y * current / aspect);
        }
    }
};

//-----------------------------------------------------------------------------------------------------------
// IRenderJob
//-----------------------------------------------------------------------------------------------------------

static RegisterEnum<AnimationType> sAnimation({
    { AnimationType::SINGLE_FRAME, "single_frame", "Renders only single frame." },
    { AnimationType::FILE_SEQUENCE, "file_sequence", "Make animation from saved files." },
});

static void addOutputCategory(VirtualSettings& connector, Path& directory, String& fileMask) {
    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect("Directory", "directory", directory)
        .setPathType(IVirtualEntry::PathType::DIRECTORY)
        .setTooltip("Directory where the images are saved.");
    outputCat.connect("File mask", "file_mask", fileMask)
        .setTooltip(
            "File mask of the created images. Can contain wildcard %d, which is replaced with the number of "
            "the saved image");
}

static void addAnimationCategory(VirtualSettings& connector, EnumWrapper& type, SequenceParams& sequence) {
    auto sequenceEnabler = [&type] { return AnimationType(type) == AnimationType::FILE_SEQUENCE; };

    VirtualSettings::Category& animationCat = connector.addCategory("Animation");
    animationCat.connect<EnumWrapper>("Animation type", "animation_type", type);
    animationCat.connect<Path>("First file", "first_file", sequence.firstFile)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats(getInputFormats())
        .setEnabler(sequenceEnabler);
    animationCat.connect("Interpolated frames", "extra_frames", sequence.extraFrames)
        .setEnabler(sequenceEnabler)
        .setTooltip("Sets the number of extra frames added between each two state files.");
}

void IRenderJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    /// \todo maybe also work with a copy of Gui ?
    gui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f));
    gui.set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 0);
    int iterLimit = 1;
    if (gui.get<RendererEnum>(GuiSettingsId::RENDERER) != RendererEnum::PARTICLE) {
        iterLimit = gui.get<int>(GuiSettingsId::RAYTRACE_ITERATION_LIMIT);
    }

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    AutoPtr<IRenderer> renderer = this->getRenderer(global);
    RawPtr<IRenderer> rendererPtr = renderer.get();

    RenderParams params = this->getRenderParams(gui);

    int firstIndex = 0;
    if (AnimationType(animationType) == AnimationType::FILE_SEQUENCE) {
        Optional<Size> sequenceFirstIndex = OutputFile::getDumpIdx(sequence.firstFile);
        if (sequenceFirstIndex) {
            firstIndex = sequenceFirstIndex.value();
        }
    }
    OutputFile paths(directory / Path(fileMask), firstIndex);
    Movie movie(gui, std::move(renderer), std::move(params), sequence.extraFrames, paths);

    switch (AnimationType(animationType)) {
    case AnimationType::SINGLE_FRAME: {
        SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");
        AnimationRenderOutput output(callbacks, *rendererPtr, iterLimit);
        movie.render(std::move(data->storage), std::move(data->stats), output);
        break;
    }
    case AnimationType::FILE_SEQUENCE: {
        FlatMap<Size, Path> fileMap = getFileSequence(sequence.firstFile);
        if (fileMap.empty()) {
            throw InvalidSetup("No files to render.");
        }

        const Size iterationCnt = iterLimit * fileMap.size() * (sequence.extraFrames + 1);
        AnimationRenderOutput output(callbacks, *rendererPtr, iterationCnt);
        AutoPtr<IInput> input = Factory::getInput(sequence.firstFile);
        for (auto& element : fileMap) {
            Storage frame;
            Statistics stats;
            const Outcome result = input->load(element.value(), frame, stats);
            if (!result) {
                /// \todo how to report this? (don't do modal dialog)
            }

            if (callbacks.shouldAbortRun()) {
                break;
            }

            movie.render(std::move(frame), std::move(stats), output);
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

RenderParams IRenderJob::getRenderParams(const GuiSettings& gui) const {
    SharedPtr<CameraData> camera = getInput<CameraData>("camera");
    RenderParams params;
    params.camera = camera->camera->clone();
    params.tracker = std::move(camera->tracker);
    GuiSettings paramGui = gui;
    paramGui.addEntries(camera->overrides);
    params.initialize(paramGui);
    return params;
}

AutoPtr<IRenderPreview> IRenderJob::getRenderPreview(const RunSettings& global) const {
    if (AnimationType(animationType) != AnimationType::SINGLE_FRAME) {
        throw InvalidSetup("Only enabled for single-frame renders");
    }

    if (!inputs.contains("particles")) {
        throw InvalidSetup("Particles not connected");
    }

    RenderParams params = this->getRenderParams();

    AutoPtr<IRenderer> renderer = this->getRenderer(global);

    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    return makeAuto<RenderPreview>(std::move(params), std::move(renderer), data);
}

RenderParams IRenderJob::getRenderParams() const {
    GuiSettings previewGui = gui;
    previewGui.set(GuiSettingsId::SHOW_KEY, false);
    previewGui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f));
    return this->getRenderParams(previewGui);
}

//-----------------------------------------------------------------------------------------------------------
// ParticleRenderJob
//-----------------------------------------------------------------------------------------------------------

ParticleRenderJob::ParticleRenderJob(const String& name)
    : IRenderJob(name) {
    gui.set(GuiSettingsId::RENDERER, RendererEnum::PARTICLE);
    animationType = EnumWrapper(AnimationType::SINGLE_FRAME);
    colorizerId = EnumWrapper(RenderColorizerId::VELOCITY);
}

UnorderedMap<String, ExtJobType> ParticleRenderJob::requires() const {
    if (AnimationType(animationType) == AnimationType::FILE_SEQUENCE &&
        RenderColorizerId(colorizerId) != RenderColorizerId::GRAVITY) {
        return { { "camera", GuiJobType::CAMERA } };
    } else {
        return this->getSlots();
    }
}

VirtualSettings ParticleRenderJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    addOutputCategory(connector, directory, fileMask);

    VirtualSettings::Category& rendererCat = connector.addCategory("Rendering");
    rendererCat.connect("Quantity", "quantity", colorizerId);
    rendererCat.connect<bool>("Transparent background", "transparent", transparentBackground);
    rendererCat.connect<Float>("Particle radius", gui, GuiSettingsId::PARTICLE_RADIUS);
    rendererCat.connect<bool>("Antialiasing", gui, GuiSettingsId::ANTIALIASED);

    addAnimationCategory(connector, animationType, sequence);

    return connector;
}

AutoPtr<IRenderer> ParticleRenderJob::getRenderer(const RunSettings& global) const {
    GuiSettings previewGui = gui;
    previewGui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f));
    AutoPtr<IColorizer> colorizer = this->getColorizer(global);
    AutoPtr<ParticleRenderer> renderer = makeAuto<ParticleRenderer>(previewGui, std::move(colorizer));
    return renderer;
}

AutoPtr<IColorizer> ParticleRenderJob::getColorizer(const RunSettings& global) const {
    Project project = Project::getInstance().clone();
    project.getGuiSettings() = gui;
    RenderColorizerId renderId(colorizerId);
    if (renderId == RenderColorizerId::GRAVITY) {
        ColorLut palette;
        if (!project.getColorLut("Acceleration", palette)) {
            palette = Factory::getColorLut(ColorizerId::ACCELERATION);
        }
        SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
        SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");
        Float G = Constants::gravity;
        if (data->overrides.has(RunSettingsId::GRAVITY_CONSTANT)) {
            G = data->overrides.get<Float>(RunSettingsId::GRAVITY_CONSTANT);
        }
        return makeAuto<GravityColorizer>(scheduler, palette, G, addSurfaceGravity);
    } else {
        return Factory::getColorizer(project, ColorizerId(renderId));
    }
}

JobRegistrar sRegisterParticleRenderer(
    "particle renderer",
    "rendering",
    [](const String& name) { return makeAuto<ParticleRenderJob>(name); },
    "Renders an image or a sequence of images. Particles are drawn as spheres using given color palette.");

//-----------------------------------------------------------------------------------------------------------
// RaytracerJob
//-----------------------------------------------------------------------------------------------------------

static RegisterEnum<ShaderFlag> sShaders({
    { ShaderFlag::SURFACENESS, "surfaceness", "Fraction of surface" },
    { ShaderFlag::EMISSION, "emission", "Emission" },
    { ShaderFlag::SCATTERING, "scattering", "Scattering" },
    { ShaderFlag::ABSORPTION, "absorption", "Absorption" },
});

RaytracerJob::RaytracerJob(const String& name)
    : IRenderJob(name) {
    gui.set(GuiSettingsId::RENDERER, RendererEnum::RAYTRACER);
    animationType = EnumWrapper(AnimationType::SINGLE_FRAME);
}

UnorderedMap<String, ExtJobType> RaytracerJob::requires() const {
    UnorderedMap<String, ExtJobType> slots;
    slots.insert("particles", JobType::PARTICLES);
    slots.insert("camera", GuiJobType::CAMERA);
    if (shaderFlags.has(ShaderFlag::SURFACENESS)) {
        slots.insert("surfaceness", GuiJobType::SHADER);
    }
    if (shaderFlags.has(ShaderFlag::EMISSION)) {
        slots.insert("emission", GuiJobType::SHADER);
    }
    if (shaderFlags.has(ShaderFlag::SCATTERING)) {
        slots.insert("scattering", GuiJobType::SHADER);
    }
    if (shaderFlags.has(ShaderFlag::ABSORPTION)) {
        slots.insert("absorption", GuiJobType::SHADER);
    }
    return slots;
}

VirtualSettings RaytracerJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    addOutputCategory(connector, directory, fileMask);

    VirtualSettings::Category& shaderCat = connector.addCategory("Shaders");
    shaderCat.connect("Used shaders", "shaders", shaderFlags);

    VirtualSettings::Category& rendererCat = connector.addCategory("Rendering");
    rendererCat.connect<int>("Interation count", gui, GuiSettingsId::RAYTRACE_ITERATION_LIMIT);
    rendererCat.connect<bool>("Transparent background", "transparent", transparentBackground);
    rendererCat.connect<Float>("Surface level", gui, GuiSettingsId::SURFACE_LEVEL);
    rendererCat.connect<Vector>("Sun position", gui, GuiSettingsId::SURFACE_SUN_POSITION);
    rendererCat.connect<Float>("Sunlight intensity", gui, GuiSettingsId::SURFACE_SUN_INTENSITY);
    rendererCat.connect<Float>("Ambient intensity", gui, GuiSettingsId::SURFACE_AMBIENT);
    rendererCat.connect<bool>("Enable shadows", gui, GuiSettingsId::RAYTRACE_SHADOWS);
    rendererCat.connect<Float>("Max distention", gui, GuiSettingsId::VOLUME_MAX_DISTENTION);

    VirtualSettings::Category& textureCat = connector.addCategory("Texture paths");
    textureCat.connect<Path>("Background", gui, GuiSettingsId::RAYTRACE_HDRI)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE);

    VirtualSettings::Category& postCat = connector.addCategory("Postprocessing");
    postCat.connect<EnumWrapper>("Color mapping", gui, GuiSettingsId::COLORMAP_TYPE);
    postCat.connect<Float>("Logarithmic factor", gui, GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR)
        .setEnabler(
            [&] { return gui.get<ColorMapEnum>(GuiSettingsId::COLORMAP_TYPE) == ColorMapEnum::LOGARITHMIC; });
    postCat.connect<bool>("Reduce noise", gui, GuiSettingsId::REDUCE_LOWFREQUENCY_NOISE);
    postCat.connect<Float>("Bloom intensity", gui, GuiSettingsId::BLOOM_INTENSITY);

    addAnimationCategory(connector, animationType, sequence);
    return connector;
}

AutoPtr<IRenderer> RaytracerJob::getRenderer(const RunSettings& global) const {
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);

    GuiSettings previewGui = gui;
    previewGui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f));
    previewGui.set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 4);
    AutoPtr<Raytracer> renderer = makeAuto<Raytracer>(scheduler, previewGui);
    if (shaderFlags.has(ShaderFlag::SURFACENESS)) {
        renderer->setSurfaceShader(this->getInput<IShader>("surfaceness"));
    }
    if (shaderFlags.has(ShaderFlag::EMISSION)) {
        renderer->setEmissionShader(this->getInput<IShader>("emission"));
    }
    if (shaderFlags.has(ShaderFlag::SCATTERING)) {
        renderer->setScatteringShader(this->getInput<IShader>("scattering"));
    }
    if (shaderFlags.has(ShaderFlag::ABSORPTION)) {
        renderer->setAbsorptionShader(this->getInput<IShader>("absorption"));
    }
    return renderer;
}

JobRegistrar sRegisterRaytracer(
    "raytracer",
    "rendering",
    [](const String& name) { return makeAuto<RaytracerJob>(name); },
    "Renders an image or a sequence of images using raytracing.");

//-----------------------------------------------------------------------------------------------------------
// VdbJob
//-----------------------------------------------------------------------------------------------------------

#ifdef SPH_USE_VDB


INLINE openvdb::Vec3f vectorToVec3f(const Vector& v) {
    return openvdb::Vec3f(v[X], v[Y], v[Z]);
}

INLINE Vector worldToRelative(const Vector& r, const Box& box, const Indices& dims) {
    return (r - box.lower()) / box.size() * Vector(dims);
}

INLINE Vector relativeToWorld(const Vector& r, const Box& box, const Indices& dims) {
    return r * box.size() / Vector(dims) + box.lower();
}

Tuple<Indices, Indices> getParticleBox(const Vector& r, const Box& box, const Indices& dims) {
    const Vector from = worldToRelative(r - Vector(2._f * r[H]), box, dims);
    const Vector to = worldToRelative(r + Vector(2._f * r[H]), box, dims);
    const Indices fromIdxs(ceil(from[X]), ceil(from[Y]), ceil(from[Z]));
    const Indices toIdxs(floor(to[X]), floor(to[Y]), floor(to[Z]));
    return { max(fromIdxs, Indices(0._f)), min(toIdxs, dims - Indices(1)) };
}

VirtualSettings VdbJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& gridCat = connector.addCategory("Grid parameters");
    gridCat.connect("Grid start [km]", "grid_start", gridStart)
        .setUnits(1.e3_f)
        .setTooltip("Sets the lower bound of the bounding box.");
    gridCat.connect("Grid end [km]", "grid_end", gridEnd)
        .setUnits(1.e3_f)
        .setTooltip("Sets the upper bound of the bounding box.");
    gridCat.connect("Resolution power", "power", dimPower)
        .setTooltip("Defines resolution of the grid. The number of voxels in one dimension is 2^power.");
    gridCat.connect("Surface level", "surface_level", surfaceLevel).setTooltip("Iso-value of the surface.");

    VirtualSettings::Category& inputCat = connector.addCategory("File sequence");
    inputCat.connect("Enable", "enable_sequence", sequence.enabled);
    inputCat.connect("First file", "first_file", sequence.firstFile)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats(getInputFormats())
        .setEnabler([this] { return sequence.enabled; });

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect("VDB File", "file", path)
        .setPathType(IVirtualEntry::PathType::OUTPUT_FILE)
        .setFileFormats({ { "OpenVDB grid file", "vdb" } })
        .setEnabler([this] { return !sequence.enabled; });

    return connector;
}

void VdbJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    openvdb::initialize();
    auto deinit = finally([] { openvdb::uninitialize(); });

    if (sequence.enabled) {
        FlatMap<Size, Path> fileMap = getFileSequence(sequence.firstFile);
        if (fileMap.empty()) {
            throw InvalidSetup("No files to render.");
        }
        const Size firstKey = fileMap.begin()->key();

        AutoPtr<IInput> input = Factory::getInput(sequence.firstFile);
        for (auto& element : fileMap) {
            Storage storage;
            Statistics stats;
            const Outcome result = input->load(element.value(), storage, stats);
            if (!result) {
                /// \todo how to report this? (don't do modal dialog)
            }

            Path outputPath = element.value();
            outputPath.replaceExtension("vdb");
            this->generate(storage, global, outputPath);

            /// \todo deduplicate with AnimationJob
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(element.key() - firstKey) / fileMap.size());
            if (element.key() == firstKey) {
                callbacks.onSetUp(storage, stats);
            }
            callbacks.onTimeStep(storage, stats);

            if (callbacks.shouldAbortRun()) {
                break;
            }
        }
    } else {
        Storage& storage = getInput<ParticleData>("particles")->storage;
        this->generate(storage, global, path);
    }
}

void VdbJob::generate(Storage& storage, const RunSettings& global, const Path& outputPath) {
    using namespace openvdb;

    FloatGrid::Ptr colorField = FloatGrid::create(-surfaceLevel);
    Vec3SGrid::Ptr velocityField = Vec3SGrid::create(vectorToVec3f(Vector(0._f)));
    FloatGrid::Ptr energyField = FloatGrid::create(0._f);

    colorField->setName("Density");
    velocityField->setName("Velocity");
    energyField->setName("Emission");

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);

    const Box box(gridStart, gridEnd);
    const Size gridSize = 1 << dimPower;
    const Indices gridIdxs(gridSize);

    LutKernel<3> kernel = Factory::getKernel<3>(global);

    typename FloatGrid::Accessor colorAccessor = colorField->getAccessor();
    typename Vec3SGrid::Accessor velocityAccessor = velocityField->getAccessor();
    typename FloatGrid::Accessor energyAccessor = energyField->getAccessor();
    for (Size i = 0; i < r.size(); ++i) {
        Indices from, to;
        tieToTuple(from, to) = getParticleBox(r[i], box, gridIdxs);
        Float rho_i;
        if (storage.getMaterialCnt() > 0) {
            rho_i = storage.getMaterialOfParticle(i)->getParam<Float>(BodySettingsId::DENSITY);
        } else {
            rho_i = rho[i];
        }
        for (int x = from[X]; x <= to[X]; ++x) {
            for (int y = from[Y]; y <= to[Y]; ++y) {
                for (int z = from[Z]; z <= to[Z]; ++z) {
                    const Indices idxs(x, y, z);
                    const Vector pos = relativeToWorld(idxs, box, gridIdxs);
                    const Float w = kernel.value(r[i] - pos, r[i][H]);
                    const Float c = m[i] / rho_i * w;

                    const Coord coord(x, y, z);
                    colorAccessor.modifyValue(coord, [c](float& color) { color += c; });
                    energyAccessor.modifyValue(coord, [&u, c, i](float& energy) { energy += c * u[i]; });
                    velocityAccessor.modifyValue(
                        coord, [&v, c, i](Vec3f& velocity) { velocity += c * vectorToVec3f(v[i]); });
                }
            }
        }
    }

    for (FloatGrid::ValueOnIter iter = colorField->beginValueOn(); iter; ++iter) {
        const Coord coord = iter.getCoord();
        const float c = *iter;
        if (c > 0) {
            energyAccessor.modifyValue(coord, [c](float& energy) { energy /= c; });
            velocityAccessor.modifyValue(coord, [c](Vec3f& velocity) { velocity /= c; });
        }
        iter.setValue(c - surfaceLevel);
    }

    GridPtrVec vdbGrids;
    vdbGrids.push_back(colorField);
    vdbGrids.push_back(velocityField);
    vdbGrids.push_back(energyField);

    Path vdbPath = outputPath;
    vdbPath.replaceExtension("vdb");
    io::File vdbFile(vdbPath.native());
    vdbFile.write(vdbGrids);
    vdbFile.close();
}

JobRegistrar sRegisterVdb(
    "save VDB grid",
    "grid",
    "rendering",
    [](const String& name) { return makeAuto<VdbJob>(name); },
    "Converts the particle data into a volumetric grid in OpenVDB format.");

#endif

NAMESPACE_SPH_END
