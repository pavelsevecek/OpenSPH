#include "gui/objects/RenderWorkers.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "run/IRun.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

enum class AnimationType {
    SINGLE_FRAME,
    ORBIT,
};

static RegisterEnum<AnimationType> sAnimation({
    { AnimationType::SINGLE_FRAME, "single_frame", "Renders only single frame." },
    { AnimationType::ORBIT, "orbit", "Make animation by orbiting camera around specified center point." },
});

static RegisterEnum<ColorizerFlag> sColorizers({
    { ColorizerFlag::VELOCITY, "velocity", "Particle velocities" },
    { ColorizerFlag::ENERGY, "energy", "Specific internal energy" },
    { ColorizerFlag::BOUND_COMPONENT_ID, "bound components", "Components" },
});

AnimationWorker::AnimationWorker(const std::string& name)
    : IParticleWorker(name) {
    animationType = EnumWrapper(AnimationType::SINGLE_FRAME);
    gui.set(GuiSettingsId::IMAGES_SAVE, true);
}

VirtualSettings AnimationWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<Path>("Directory", gui, GuiSettingsId::IMAGES_PATH)
        .connect<std::string>("File mask", gui, GuiSettingsId::IMAGES_NAME);

    auto particleEnabler = [this] {
        return gui.get<RendererEnum>(GuiSettingsId::RENDERER) == RendererEnum::PARTICLE;
    };

    VirtualSettings::Category& rendererCat = connector.addCategory("Rendering");
    rendererCat.connect<EnumWrapper>("Renderer", gui, GuiSettingsId::RENDERER)
        .connect("Quantities", "quantities", colorizers)
        .connect<int>("Image width", gui, GuiSettingsId::IMAGES_WIDTH)
        .connect<int>("Image height", gui, GuiSettingsId::IMAGES_HEIGHT)
        .connect<Float>("Particle radius", gui, GuiSettingsId::PARTICLE_RADIUS, particleEnabler)
        .connect<bool>("Antialiasing", gui, GuiSettingsId::ANTIALIASED, particleEnabler);

    VirtualSettings::Category& cameraCat = connector.addCategory("Camera");
    cameraCat.connect<EnumWrapper>("Camera type", gui, GuiSettingsId::CAMERA)
        .connect<Float>("Field of view", gui, GuiSettingsId::PERSPECTIVE_FOV, DEG_TO_RAD)
        .connect<Vector>("Position", gui, GuiSettingsId::PERSPECTIVE_POSITION)
        .connect<Vector>("Target", gui, GuiSettingsId::PERSPECTIVE_TARGET)
        .connect<Vector>("Up-direction", gui, GuiSettingsId::PERSPECTIVE_UP)
        .connect<Float>("Clip near", gui, GuiSettingsId::PERSPECTIVE_CLIP_NEAR)
        .connect<Float>("Clip far", gui, GuiSettingsId::PERSPECTIVE_CLIP_FAR);

    VirtualSettings::Category& animationCat = connector.addCategory("Animation");
    animationCat.connect<EnumWrapper>("Animation type", "animation_type", animationType)
        .connect<Float>("Angular step", "step", step, DEG_TO_RAD)
        .connect<Float>("Final angle", "final_angle", finalAngle, DEG_TO_RAD);

    return connector;
}

void AnimationWorker::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    AutoPtr<IRenderer> renderer = Factory::getRenderer(*scheduler, gui);

    RenderParams params;
    params.size = { gui.get<int>(GuiSettingsId::IMAGES_WIDTH), gui.get<int>(GuiSettingsId::IMAGES_HEIGHT) };
    params.camera = Factory::getCamera(gui, params.size);
    params.initialize(gui);

    Project project;
    project.getGuiSettings() = gui;
    Array<SharedPtr<IColorizer>> colorizerArray;
    if (colorizers.has(ColorizerFlag::VELOCITY)) {
        colorizerArray.push(Factory::getColorizer(project, ColorizerId::VELOCITY));
    }
    if (colorizers.has(ColorizerFlag::ENERGY)) {
        colorizerArray.push(Factory::getColorizer(project, ColorizerId(QuantityId::ENERGY)));
    }
    if (colorizers.has(ColorizerFlag::BOUND_COMPONENT_ID)) {
        colorizerArray.push(Factory::getColorizer(project, ColorizerId::BOUND_COMPONENT_ID));
    }
    for (auto& colorizer : colorizerArray) {
        colorizer->initialize(data->storage, RefEnum::WEAK);
    }

    Movie movie(gui, std::move(renderer), std::move(colorizerArray), std::move(params));

    switch (AnimationType(animationType)) {
    case AnimationType::SINGLE_FRAME:
        movie.save(data->storage, data->stats);
        break;
    case AnimationType::ORBIT: {
        const Vector target = gui.get<Vector>(GuiSettingsId::PERSPECTIVE_TARGET);
        const Vector position = gui.get<Vector>(GuiSettingsId::PERSPECTIVE_POSITION);
        const Float orbitRadius = getLength(target - position);

        for (Float phi = 0._f; phi < finalAngle; phi += step) {
            const Vector newPosition = target + orbitRadius * (cos(phi) * Vector(0._f, 0._f, 1._f) +
                                                                  sin(phi) * Vector(1._f, 0._f, 0._f));
            gui.set(GuiSettingsId::PERSPECTIVE_POSITION, newPosition);
            movie.setCamera(Factory::getCamera(gui, params.size));
            movie.save(data->storage, data->stats);

            data->stats.set(StatisticsId::RELATIVE_PROGRESS, phi / finalAngle);
            callbacks.onTimeStep(Storage{}, data->stats);

            if (callbacks.shouldAbortRun()) {
                break;
            }
        }

        // reset the position
        gui.set(GuiSettingsId::PERSPECTIVE_POSITION, position);
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

WorkerRegistrar sRegisterAnimation("render animation", "animation", "rendering", [](const std::string& name) {
    return makeAuto<AnimationWorker>(name);
});

NAMESPACE_SPH_END
