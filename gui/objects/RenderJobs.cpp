#include "gui/objects/RenderJobs.h"
#include "gravity/BarnesHut.h"
#include "gravity/Moments.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "run/IRun.h"
#include "run/VirtualSettings.h"
#include "run/jobs/IoJobs.h"
#include "system/Factory.h"
#include "system/Timer.h"

#ifdef SPH_USE_VDB
#include <openvdb/openvdb.h>
#endif


NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// AnimationJob
//-----------------------------------------------------------------------------------------------------------

static RegisterEnum<AnimationType> sAnimation({
    { AnimationType::SINGLE_FRAME, "single_frame", "Renders only single frame." },
    { AnimationType::ORBIT, "orbit", "Make animation by orbiting camera around specified center point." },
    { AnimationType::FILE_SEQUENCE, "file_sequence", "Make animation from saved files." },
});

static RegisterEnum<ColorizerFlag> sColorizers({
    { ColorizerFlag::VELOCITY, "velocity", "Particle velocities" },
    { ColorizerFlag::ENERGY, "energy", "Specific internal energy" },
    { ColorizerFlag::BOUND_COMPONENT_ID, "bound components", "Components" },
    { ColorizerFlag::MASS, "clay", "Clay" },
    { ColorizerFlag::BEAUTY, "beauty", "Beauty" },
    { ColorizerFlag::GRAVITY, "gravity", "Gravity" },
    { ColorizerFlag::DAMAGE, "damage", "Damage" },
});

AnimationJob::AnimationJob(const std::string& name)
    : INullJob(name) {
    animationType = EnumWrapper(AnimationType::SINGLE_FRAME);
    gui.set(GuiSettingsId::IMAGES_SAVE, true);
}

VirtualSettings AnimationJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<Path>("Directory", gui, GuiSettingsId::IMAGES_PATH)
        .setPathType(IVirtualEntry::PathType::DIRECTORY);
    outputCat.connect<std::string>("File mask", gui, GuiSettingsId::IMAGES_NAME);

    auto particleEnabler = [this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::PARTICLE;
    };
    auto raymarcherEnabler = [this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::RAYMARCHER;
    };
    auto surfaceEnabler = [this] {
        const RendererEnum type = gui.get<RendererEnum>(GuiSettingsId::RENDERER);
        return type == RendererEnum::RAYMARCHER || type == RendererEnum::MESH;
    };
    auto volumeEnabler = [this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::VOLUME;
    };

    VirtualSettings::Category& rendererCat = connector.addCategory("Rendering");
    rendererCat.connect<EnumWrapper>("Renderer", gui, GuiSettingsId::RENDERER);
    rendererCat.connect("Quantities", "quantities", colorizers);
    rendererCat.connect("Include surface gravity", "surface_gravity", addSurfaceGravity)
        .setEnabler([this] { return colorizers.has(ColorizerFlag::GRAVITY); })
        .setTooltip("Include the surface gravity of the particle itself.");
    rendererCat.connect<bool>("Transparent background", "transparent", transparentBackground);
    rendererCat.connect<EnumWrapper>("Color mapping", gui, GuiSettingsId::COLORMAP_TYPE);
    rendererCat.connect<Float>("Logarithmic factor", gui, GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR)
        .setEnabler(
            [&] { return gui.get<ColorMapEnum>(GuiSettingsId::COLORMAP_TYPE) == ColorMapEnum::LOGARITHMIC; });
    rendererCat.connect<Float>("Particle radius", gui, GuiSettingsId::PARTICLE_RADIUS)
        .setEnabler(particleEnabler);
    rendererCat.connect<bool>("Antialiasing", gui, GuiSettingsId::ANTIALIASED).setEnabler(particleEnabler);
    rendererCat.connect<bool>("Show key", gui, GuiSettingsId::SHOW_KEY).setEnabler(particleEnabler);
    rendererCat.connect<int>("Interation count", gui, GuiSettingsId::RAYTRACE_ITERATION_LIMIT)
        .setEnabler([&] {
            const RendererEnum type = gui.get<RendererEnum>(GuiSettingsId::RENDERER);
            return type == RendererEnum::RAYMARCHER || type == RendererEnum::VOLUME;
        });
    rendererCat.connect<Float>("Surface level", gui, GuiSettingsId::SURFACE_LEVEL).setEnabler(surfaceEnabler);
    rendererCat.connect<Vector>("Sun position", gui, GuiSettingsId::SURFACE_SUN_POSITION)
        .setEnabler(surfaceEnabler);
    rendererCat.connect<Float>("Sunlight intensity", gui, GuiSettingsId::SURFACE_SUN_INTENSITY)
        .setEnabler(surfaceEnabler);
    rendererCat.connect<Float>("Ambient intensity", gui, GuiSettingsId::SURFACE_AMBIENT)
        .setEnabler(surfaceEnabler);
    rendererCat.connect<Float>("Surface emission", gui, GuiSettingsId::SURFACE_EMISSION)
        .setEnabler(raymarcherEnabler);
    rendererCat.connect<EnumWrapper>("BRDF", gui, GuiSettingsId::RAYTRACE_BRDF).setEnabler(raymarcherEnabler);
    rendererCat.connect<bool>("Render as spheres", gui, GuiSettingsId::RAYTRACE_SPHERES)
        .setEnabler(raymarcherEnabler);
    rendererCat.connect<bool>("Enable shadows", gui, GuiSettingsId::RAYTRACE_SHADOWS)
        .setEnabler(raymarcherEnabler);
    rendererCat.connect<Float>("Medium emission [km^-1]", gui, GuiSettingsId::VOLUME_EMISSION)
        .setUnits(1.e-3_f)
        .setEnabler(volumeEnabler);
    rendererCat.connect<Float>("Medium absorption [km^-1]", gui, GuiSettingsId::VOLUME_ABSORPTION)
        .setUnits(1.e-3_f)
        .setEnabler(volumeEnabler);
    rendererCat.connect<Float>("Cell size", gui, GuiSettingsId::SURFACE_RESOLUTION).setEnabler([this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::MESH;
    });

    VirtualSettings::Category& textureCat = connector.addCategory("Texture paths");
    textureCat.connect<Path>("Background", gui, GuiSettingsId::RAYTRACE_HDRI)
        .setEnabler([this] {
            const RendererEnum id = gui.get<RendererEnum>(GuiSettingsId::RENDERER);
            return id == RendererEnum::VOLUME || id == RendererEnum::RAYMARCHER;
        })
        .setPathType(IVirtualEntry::PathType::INPUT_FILE);

    auto orbitEnabler = [this] { return AnimationType(animationType) == AnimationType::ORBIT; };

    VirtualSettings::Category& animationCat = connector.addCategory("Animation");
    animationCat.connect<EnumWrapper>("Animation type", "animation_type", animationType);
    animationCat.connect<Float>("Angular step", "step", orbit.step)
        .setUnits(DEG_TO_RAD)
        .setEnabler(orbitEnabler);
    animationCat.connect<Float>("Final angle", "final_angle", orbit.finalAngle)
        .setUnits(DEG_TO_RAD)
        .setEnabler(orbitEnabler);
    animationCat.connect<Path>("First file", "first_file", sequence.firstFile)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats(getInputFormats())
        .setEnabler([this] { return AnimationType(animationType) == AnimationType::FILE_SEQUENCE; });

    return connector;
}

class GravityColorizer : public TypedColorizer<Float> {
private:
    SharedPtr<IScheduler> scheduler;
    BarnesHut gravity;
    Array<Float> acc;
    Float G;
    bool addSurfaceGravity;

public:
    GravityColorizer(const SharedPtr<IScheduler>& scheduler,
        const Palette& palette,
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
        gravity.evalAll(*scheduler, dv, stats);
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
        return palette(acc[idx]);
    }

    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual std::string name() const override {
        return "Gravity";
    }
};

static Array<AutoPtr<IColorizer>> getColorizers(const RunSettings& global,
    const GuiSettings& gui,
    const Flags<ColorizerFlag> colorizers,
    const Float G,
    const bool addSurfaceGravity) {
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    Project project = Project::getInstance().clone();
    project.getGuiSettings() = gui;
    Array<AutoPtr<IColorizer>> colorizerArray;
    if (colorizers.has(ColorizerFlag::VELOCITY)) {
        colorizerArray.push(Factory::getColorizer(project, ColorizerId::VELOCITY));
    }
    if (colorizers.has(ColorizerFlag::ENERGY)) {
        colorizerArray.push(Factory::getColorizer(project, QuantityId::ENERGY));
    }
    if (colorizers.has(ColorizerFlag::BOUND_COMPONENT_ID)) {
        colorizerArray.push(Factory::getColorizer(project, ColorizerId::BOUND_COMPONENT_ID));
    }
    if (colorizers.has(ColorizerFlag::MASS)) {
        colorizerArray.push(Factory::getColorizer(project, QuantityId::MASS));
    }
    if (colorizers.has(ColorizerFlag::BEAUTY)) {
        colorizerArray.push(Factory::getColorizer(project, ColorizerId::BEAUTY));
    }
    if (colorizers.has(ColorizerFlag::GRAVITY)) {
        Palette palette;
        if (!project.getPalette("Acceleration", palette)) {
            palette = Factory::getPalette(ColorizerId::ACCELERATION);
        }
        colorizerArray.push(makeAuto<GravityColorizer>(scheduler, palette, G, addSurfaceGravity));
    }
    if (colorizers.has(ColorizerFlag::DAMAGE)) {
        colorizerArray.push(Factory::getColorizer(project, QuantityId::DAMAGE));
    }
    return colorizerArray;
}

RenderParams AnimationJob::getRenderParams() const {
    SharedPtr<CameraData> camera = getInput<CameraData>("camera");
    RenderParams params;
    params.camera = camera->camera->clone();
    params.tracker = std::move(camera->tracker);
    GuiSettings paramGui = gui;
    paramGui.addEntries(camera->overrides);
    params.initialize(paramGui);
    return params;
}

void AnimationJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    /// \todo maybe also work with a copy of Gui ?
    gui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f));
    gui.set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 0);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    AutoPtr<IRenderer> renderer = Factory::getRenderer(scheduler, gui);
    RenderParams params = getRenderParams();
    Array<AutoPtr<IColorizer>> colorizerArray =
        getColorizers(global, gui, colorizers, Constants::gravity, addSurfaceGravity);

    if (AnimationType(animationType) == AnimationType::FILE_SEQUENCE) {
        Optional<Size> firstIndex = OutputFile::getDumpIdx(sequence.firstFile);
        if (firstIndex) {
            gui.set(GuiSettingsId::IMAGES_FIRST_INDEX, int(firstIndex.value()));
        }
    }

    Movie movie(gui, std::move(renderer), std::move(colorizerArray), std::move(params));
    Timer renderTimer;
    switch (AnimationType(animationType)) {
    case AnimationType::SINGLE_FRAME: {
        SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");
        movie.save(data->storage, data->stats);
        break;
    }
    case AnimationType::ORBIT: {
        SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");
        const Vector target = gui.get<Vector>(GuiSettingsId::CAMERA_TARGET);
        const Vector position = gui.get<Vector>(GuiSettingsId::CAMERA_POSITION);
        const Float orbitRadius = getLength(target - position);

        for (Float phi = 0._f; phi < orbit.finalAngle; phi += orbit.step) {
            const Vector newPosition = target + orbitRadius * (cos(phi) * Vector(0._f, 0._f, 1._f) +
                                                                  sin(phi) * Vector(1._f, 0._f, 0._f));
            params.camera->setPosition(newPosition);
            movie.setCamera(params.camera->clone());
            movie.save(data->storage, data->stats);

            data->stats.set(StatisticsId::RELATIVE_PROGRESS, phi / orbit.finalAngle);
            data->stats.set(StatisticsId::WALLCLOCK_TIME, int(renderTimer.elapsed(TimerUnit::MILLISECOND)));
            callbacks.onTimeStep(Storage{}, data->stats);

            if (callbacks.shouldAbortRun()) {
                break;
            }
        }

        // reset the position
        gui.set(GuiSettingsId::CAMERA_POSITION, position);
        break;
    }
    case AnimationType::FILE_SEQUENCE: {
        FlatMap<Size, Path> fileMap = getFileSequence(sequence.firstFile);
        if (fileMap.empty()) {
            throw InvalidSetup("No files to render.");
        }
        const Size firstKey = fileMap.begin()->key;

        AutoPtr<IInput> input = Factory::getInput(sequence.firstFile);
        for (auto& element : fileMap) {
            Storage storage;
            Statistics stats;
            const Outcome result = input->load(element.value, storage, stats);
            if (!result) {
                /// \todo how to report this? (don't do modal dialog)
            }

            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(element.key - firstKey) / fileMap.size());
            stats.set(StatisticsId::WALLCLOCK_TIME, int(renderTimer.elapsed(TimerUnit::MILLISECOND)));
            if (element.key == firstKey) {
                callbacks.onSetUp(storage, stats);
            }
            callbacks.onTimeStep(storage, stats);

            if (callbacks.shouldAbortRun()) {
                break;
            }

            movie.save(storage, stats);
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

class RenderPreview : public IRenderPreview {
private:
    RenderParams params;
    AutoPtr<IRenderer> renderer;
    AutoPtr<IColorizer> colorizer;
    SharedPtr<ParticleData> data;
    std::atomic_bool cancelled;

public:
    RenderPreview(RenderParams&& params,
        AutoPtr<IRenderer>&& renderer,
        AutoPtr<IColorizer>&& colorizer,
        const SharedPtr<ParticleData>& data)
        : params(std::move(params))
        , renderer(std::move(renderer))
        , colorizer(std::move(colorizer))
        , data(data)
        , cancelled(false) {}

    virtual void render(const Pixel resolution, IRenderOutput& output) override {
        cancelled = false;

        // lazy init
        if (!colorizer->isInitialized()) {
            std::cout << "Initializing the colorizer" << std::endl;
            colorizer->initialize(data->storage, RefEnum::WEAK);
        }
        if (cancelled) {
            return;
        }
        if (!renderer->isInitialized()) {
            std::cout << "Initializing the renderer" << std::endl;
            renderer->initialize(data->storage, *colorizer, *params.camera);
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

    virtual void update(AutoPtr<IColorizer>&& newColorizer) override {
        colorizer = std::move(newColorizer);
    }

    virtual void update(AutoPtr<IRenderer>&& newRenderer) override {
        renderer = std::move(newRenderer);
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

AutoPtr<IRenderPreview> AnimationJob::getRenderPreview(const RunSettings& global) const {
    if (AnimationType(animationType) != AnimationType::SINGLE_FRAME) {
        throw InvalidSetup("Only enabled for single-frame renders");
    }

    if (!inputs.contains("particles")) {
        throw InvalidSetup("Paritcles not connected");
    }

    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");
    /*Float G = Constants::gravity;
    if (data->overrides.has(RunSettingsId::GRAVITY_CONSTANT)) {
        G = data->overrides.get<Float>(RunSettingsId::GRAVITY_CONSTANT);
    }*/

    RenderParams params = this->getRenderParams();
    AutoPtr<IColorizer> colorizer = this->getColorizer(global);
    if (!colorizer) {
        throw InvalidSetup("No quantity selected");
    }
    AutoPtr<IRenderer> renderer = this->getRenderer(global);

    return makeAuto<RenderPreview>(std::move(params), std::move(renderer), std::move(colorizer), data);
}

AutoPtr<IColorizer> AnimationJob::getColorizer(const RunSettings& global) const {
    /// \todo
    Float G = Constants::gravity;
    Array<AutoPtr<IColorizer>> colorizer = getColorizers(global, gui, colorizers, G, addSurfaceGravity);
    if (!colorizer.empty()) {
        return std::move(colorizer.front());
    } else {
        return nullptr;
    }
}

AutoPtr<IRenderer> AnimationJob::getRenderer(const RunSettings& global) const {
    GuiSettings previewGui = gui;
    previewGui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f))
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 4)
        .set(GuiSettingsId::SHOW_KEY, false);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    AutoPtr<IRenderer> renderer = Factory::getRenderer(scheduler, previewGui);
    return renderer;
}

JobRegistrar sRegisterAnimation(
    "render animation",
    "animation",
    "rendering",
    [](const std::string& name) { return makeAuto<AnimationJob>(name); },
    "Renders an image or a sequence of images from given particle input(s)");

//-----------------------------------------------------------------------------------------------------------
// VdbJob
//-----------------------------------------------------------------------------------------------------------

#ifdef SPH_USE_VDB


INLINE openvdb::Vec3R vectorToVec3R(const Vector& v) {
    return openvdb::Vec3R(v[X], v[Y], v[Z]);
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
    gridCat.connect("Grid start", "grid_start", gridStart);
    gridCat.connect("Grid end", "grid_end", gridEnd);
    gridCat.connect("Resolution power", "power", dimPower);
    gridCat.connect("Surface level", "surface_level", surfaceLevel);

    VirtualSettings::Category& inputCat = connector.addCategory("Input files");
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
        const Size firstKey = fileMap.begin()->key;

        AutoPtr<IInput> input = Factory::getInput(sequence.firstFile);
        for (auto& element : fileMap) {
            Storage storage;
            Statistics stats;
            const Outcome result = input->load(element.value, storage, stats);
            if (!result) {
                /// \todo how to report this? (don't do modal dialog)
            }

            Path outputPath = element.value;
            outputPath.replaceExtension("vdb");
            this->generate(storage, global, outputPath);

            /// \todo deduplicate with AnimationJob
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(element.key - firstKey) / fileMap.size());
            if (element.key == firstKey) {
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
    openvdb::FloatGrid::Ptr colorField = openvdb::FloatGrid::create(-surfaceLevel);
    openvdb::Vec3SGrid::Ptr velocityField = openvdb::Vec3SGrid::create(vectorToVec3R(Vector(0._f)));
    openvdb::FloatGrid::Ptr energyField = openvdb::FloatGrid::create(0._f);

    colorField->setName("Density");
    velocityField->setName("Velocity");
    energyField->setName("Emission");

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    // ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);


    const Box box(gridStart, gridEnd);
    const Size gridSize = 1 << dimPower;
    const Indices gridIdxs(gridSize);

    LutKernel<3> kernel = Factory::getKernel<3>(global);

    typename openvdb::FloatGrid::Accessor colorAccessor = colorField->getAccessor();
    typename openvdb::Vec3SGrid::Accessor velocityAccessor = velocityField->getAccessor();
    typename openvdb::FloatGrid::Accessor energyAccessor = energyField->getAccessor();
    for (Size i = 0; i < r.size(); ++i) {
        Indices from, to;
        tieToTuple(from, to) = getParticleBox(r[i], box, gridIdxs);
        const Float rho = storage.getMaterialOfParticle(i)->getParam<Float>(BodySettingsId::DENSITY);
        for (int x = from[X]; x <= to[X]; ++x) {
            for (int y = from[Y]; y <= to[Y]; ++y) {
                for (int z = from[Z]; z <= to[Z]; ++z) {
                    const Indices idxs(x, y, z);
                    const Vector pos = relativeToWorld(idxs, box, gridIdxs);
                    const Float w = kernel.value(r[i] - pos, r[i][H]);
                    const Float p = m[i] / rho;


                    const openvdb::Coord coord(x, y, z);
                    colorAccessor.modifyValue(coord, [p, w](float& color) { color += p * w; });
                    energyAccessor.modifyValue(
                        coord, [p, w, &u, i](float& energy) { energy += p * w * u[i]; });
                    /*velocityAccessor.modifyValue(coord,
                        [p, w, &v, i](openvdb::Vec3R& velocity) { velocity += vectorToVec3R(p * w * v[i]);
                       });*/
                }
            }
        }
    }

    openvdb::GridPtrVec vdbGrids;
    vdbGrids.push_back(colorField);
    // vdbGrids.push_back(velocityField);
    vdbGrids.push_back(energyField);

    Path vdbPath = outputPath;
    vdbPath.replaceExtension("vdb");
    openvdb::io::File vdbFile(vdbPath.native());
    vdbFile.write(vdbGrids);
    vdbFile.close();
}

JobRegistrar sRegisterVdb(
    "save VDB grid",
    "grid",
    "rendering",
    [](const std::string& name) { return makeAuto<VdbJob>(name); },
    "Converts the particle data into a volumetric grid in OpenVDB format.");

#endif

NAMESPACE_SPH_END
