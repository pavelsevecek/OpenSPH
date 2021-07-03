#include "gui/objects/RenderJobs.h"
#include "gravity/BarnesHut.h"
#include "gravity/Moments.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "run/IRun.h"
#include "run/workers/IoJobs.h"
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
    outputCat.connect<Path>("Directory", gui, GuiSettingsId::IMAGES_PATH);
    outputCat.connect<std::string>("File mask", gui, GuiSettingsId::IMAGES_NAME);

    auto particleEnabler = [this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::PARTICLE;
    };
    auto raytracerEnabler = [this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::RAYMARCHER;
    };
    auto surfaceEnabler = [this] {
        const RendererEnum type = gui.get<RendererEnum>(GuiSettingsId::RENDERER);
        return type == RendererEnum::RAYMARCHER || type == RendererEnum::MESH;
    };

    VirtualSettings::Category& rendererCat = connector.addCategory("Rendering");
    rendererCat.connect<EnumWrapper>("Renderer", gui, GuiSettingsId::RENDERER);
    rendererCat.connect("Quantities", "quantities", colorizers);
    rendererCat.connect<bool>("Transparent background", "transparent", transparentBackground);
    rendererCat.connect<EnumWrapper>("Color mapping", gui, GuiSettingsId::COLORMAP);
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
        .setEnabler(raytracerEnabler);
    rendererCat.connect<EnumWrapper>("BRDF", gui, GuiSettingsId::RAYTRACE_BRDF).setEnabler(raytracerEnabler);
    rendererCat.connect<bool>("Render as spheres", gui, GuiSettingsId::RAYTRACE_SPHERES)
        .setEnabler(raytracerEnabler);
    rendererCat.connect<Float>("Medium emission [km^-1]", gui, GuiSettingsId::VOLUME_EMISSION)
        .setUnits(1.e-3_f)
        .setEnabler(
            [this] { return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::VOLUME; });
    rendererCat.connect<Float>("Cell size", gui, GuiSettingsId::SURFACE_RESOLUTION).setEnabler([this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::MESH;
    });

    VirtualSettings::Category& textureCat = connector.addCategory("Texture paths");
    textureCat.connect<std::string>("Background", gui, GuiSettingsId::RAYTRACE_HDRI)
        .setEnabler(raytracerEnabler);

    auto orbitEnabler = [this] { return AnimationType(animationType) == AnimationType::ORBIT; };

    VirtualSettings::Category& animationCat = connector.addCategory("Animation");
    animationCat.connect<EnumWrapper>("Animation type", "animation_type", animationType);
    animationCat.connect<Float>("Angular step", "step", orbit.step)
        .setUnits(DEG_TO_RAD)
        .setEnabler(orbitEnabler);
    animationCat.connect<Float>("Final angle", "final_angle", orbit.finalAngle)
        .setUnits(DEG_TO_RAD)
        .setEnabler(orbitEnabler);
    animationCat.connect<Path>("First file", "first_file", sequence.firstFile).setEnabler([this] {
        return AnimationType(animationType) == AnimationType::FILE_SEQUENCE;
    });

    return connector;
}

class GravityColorizer : public TypedColorizer<Vector> {
private:
    SharedPtr<IScheduler> scheduler;
    BarnesHut gravity;
    Array<Vector> dv;

public:
    explicit GravityColorizer(const SharedPtr<IScheduler>& scheduler, Palette palette)
        : TypedColorizer<Vector>(QuantityId::POSITION, std::move(palette))
        , scheduler(scheduler)
        , gravity(0.8_f, MultipoleOrder::OCTUPOLE) {}

    virtual void initialize(const Storage& storage, const RefEnum UNUSED(ref)) override {
        gravity.build(*scheduler, storage);
        Statistics stats;
        dv.resize(storage.getParticleCnt());
        dv.fill(Vector(0._f));
        gravity.evalAll(*scheduler, dv, stats);
    }

    virtual Rgba evalColor(const Size idx) const override {
        return palette(getLength(dv[idx]));
    }

    virtual Optional<Vector> evalVector(const Size UNUSED(idx)) const override {
        return NOTHING;
    }

    virtual std::string name() const override {
        return "Gravity";
    }
};

void AnimationJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    gui.set(GuiSettingsId::BACKGROUND_COLOR, Rgba(0.f, 0.f, 0.f, transparentBackground ? 0.f : 1.f));

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    AutoPtr<IRenderer> renderer = Factory::getRenderer(scheduler, gui);

    SharedPtr<ICamera> camera = getInput<ICamera>("camera");
    RenderParams params;
    params.size = camera->getSize();
    params.camera = camera->clone();
    params.tracker = Factory::getTracker(gui);
    params.initialize(gui);

    Project project = Project::getInstance().clone();
    project.getGuiSettings() = gui;
    Array<SharedPtr<IColorizer>> colorizerArray;
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
        colorizerArray.push(makeShared<GravityColorizer>(scheduler, palette));
    }
    if (colorizers.has(ColorizerFlag::DAMAGE)) {
        colorizerArray.push(Factory::getColorizer(project, QuantityId::DAMAGE));
    }

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
            gui.set(GuiSettingsId::CAMERA_POSITION, newPosition);
            movie.setCamera(Factory::getCamera(gui, params.size));
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

JobRegistrar sRegisterAnimation(
    "render animation",
    "animation",
    "rendering",
    [](const std::string& name) { return makeAuto<AnimationJob>(name); },
    "Renders an image or a sequence of images from given particle input(s)");

//-----------------------------------------------------------------------------------------------------------
// VdbWorker
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
    inputCat.connect("First file", "first_file", sequence.firstFile).setEnabler([this] {
        return sequence.enabled;
    });

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect("VDB File", "file", path).setEnabler([this] { return !sequence.enabled; });

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

            /// \todo deduplicate with AnimationWorker
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
