#include "gui/objects/CameraJobs.h"
#include "gui/Factory.h"

NAMESPACE_SPH_BEGIN


void ICameraJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    const int width = gui.get<int>(GuiSettingsId::IMAGES_WIDTH);
    const int height = gui.get<int>(GuiSettingsId::IMAGES_HEIGHT);
    camera = Factory::getCamera(gui, Pixel(width, height));
}

OrthoCameraJob::OrthoCameraJob(const std::string& name)
    : ICameraJob(name) {
    gui.set(GuiSettingsId::CAMERA_TYPE, CameraEnum::ORTHO);
}

static VirtualSettings::Category& addResolutionCategory(VirtualSettings& connector, GuiSettings& gui) {
    VirtualSettings::Category& resCat = connector.addCategory("Resolution");
    resCat.connect<int>("Image width", gui, GuiSettingsId::IMAGES_WIDTH);
    resCat.connect<int>("Image height", gui, GuiSettingsId::IMAGES_HEIGHT);
    return resCat;
}

static VirtualSettings::Category& addTransformCategory(VirtualSettings& connector, GuiSettings& gui) {
    VirtualSettings::Category& transformCat = connector.addCategory("Transform");
    transformCat.connect<Vector>("Position", gui, GuiSettingsId::CAMERA_POSITION);
    transformCat.connect<Vector>("Velocity", gui, GuiSettingsId::CAMERA_VELOCITY);
    transformCat.connect<Vector>("Target", gui, GuiSettingsId::CAMERA_TARGET);
    transformCat.connect<Vector>("Up-direction", gui, GuiSettingsId::CAMERA_UP);
    transformCat.connect<Float>("Clip near", gui, GuiSettingsId::CAMERA_CLIP_NEAR);
    transformCat.connect<Float>("Clip far", gui, GuiSettingsId::CAMERA_CLIP_FAR);
    return transformCat;
}

static void addTrackingCategory(VirtualSettings& connector, GuiSettings& gui) {
    VirtualSettings::Category& trackingCat = connector.addCategory("Tracking");
    trackingCat.connect<int>("Track particle", gui, GuiSettingsId::CAMERA_TRACK_PARTICLE);
    trackingCat.connect<bool>("Track median", gui, GuiSettingsId::CAMERA_TRACK_MEDIAN).setEnabler([gui] {
        return gui.get<int>(GuiSettingsId::CAMERA_TRACK_PARTICLE) == -1;
    });
    trackingCat.connect<Vector>("Tracking offset", gui, GuiSettingsId::CAMERA_TRACKING_OFFSET)
        .setEnabler([gui] { return gui.get<bool>(GuiSettingsId::CAMERA_TRACK_MEDIAN); });
}

VirtualSettings OrthoCameraJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addResolutionCategory(connector, gui);
    VirtualSettings::Category& transformCat = addTransformCategory(connector, gui);
    transformCat.connect<Float>("Ortho FoV [km]", gui, GuiSettingsId::CAMERA_ORTHO_FOV).setUnits(1.e3_f);
    transformCat.connect<Float>("Cutoff distance [km]", gui, GuiSettingsId::CAMERA_ORTHO_CUTOFF)
        .setUnits(1.e3_f);
    addTrackingCategory(connector, gui);
    return connector;
}


JobRegistrar sRegisterOrtho(
    "orthographic camera",
    "camera",
    "rendering",
    [](const std::string& name) { return makeAuto<OrthoCameraJob>(name); },
    "Creates an orthographic camera");

PerspectiveCameraJob::PerspectiveCameraJob(const std::string& name)
    : ICameraJob(name) {
    gui.set(GuiSettingsId::CAMERA_TYPE, CameraEnum::PERSPECTIVE);
}

VirtualSettings PerspectiveCameraJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addResolutionCategory(connector, gui);
    VirtualSettings::Category& transformCat = addTransformCategory(connector, gui);
    transformCat.connect<Float>("Field of view [deg]", gui, GuiSettingsId::CAMERA_PERSPECTIVE_FOV)
        .setUnits(DEG_TO_RAD);

    addTrackingCategory(connector, gui);

    return connector;
}

JobRegistrar sRegisterPerspective(
    "perspective camera",
    "camera",
    "rendering",
    [](const std::string& name) { return makeAuto<PerspectiveCameraJob>(name); },
    "Creates a perspective (pinhole) camera.");

NAMESPACE_SPH_END
