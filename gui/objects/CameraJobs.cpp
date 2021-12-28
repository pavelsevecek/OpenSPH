#include "gui/objects/CameraJobs.h"
#include "gui/Factory.h"

NAMESPACE_SPH_BEGIN

void ICameraJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    const int width = gui.get<int>(GuiSettingsId::CAMERA_WIDTH);
    const int height = gui.get<int>(GuiSettingsId::CAMERA_HEIGHT);
    AutoPtr<ICamera> camera = Factory::getCamera(gui, Pixel(width, height));
    AutoPtr<ITracker> tracker = Factory::getTracker(gui);

    result = makeShared<CameraData>();
    result->camera = std::move(camera);
    result->tracker = std::move(tracker);
    result->overrides.set(GuiSettingsId::CAMERA_VELOCITY, gui.get<Vector>(GuiSettingsId::CAMERA_VELOCITY));
    result->overrides.set(GuiSettingsId::CAMERA_ORBIT, gui.get<Float>(GuiSettingsId::CAMERA_ORBIT));
}

// ----------------------------------------------------------------------------------------------------------
// OrthoCameraJob
// ----------------------------------------------------------------------------------------------------------

OrthoCameraJob::OrthoCameraJob(const String& name)
    : ICameraJob(name) {
    gui.set(GuiSettingsId::CAMERA_TYPE, CameraEnum::ORTHO);
}

static VirtualSettings::Category& addResolutionCategory(VirtualSettings& connector, GuiSettings& gui) {
    VirtualSettings::Category& resCat = connector.addCategory("Resolution");
    resCat.connect<int>("Image width", gui, GuiSettingsId::CAMERA_WIDTH);
    resCat.connect<int>("Image height", gui, GuiSettingsId::CAMERA_HEIGHT);
    return resCat;
}

static VirtualSettings::Category& addTransformCategory(VirtualSettings& connector, GuiSettings& gui) {
    VirtualSettings::Category& transformCat = connector.addCategory("Transform");
    transformCat.connect<Vector>("Position [km]", gui, GuiSettingsId::CAMERA_POSITION).setUnits(1.e3_f);
    transformCat.connect<Vector>("Velocity [km/s]", gui, GuiSettingsId::CAMERA_VELOCITY)
        .setUnits(1.e3_f)
        .setEnabler([&gui] {
            return gui.get<int>(GuiSettingsId::CAMERA_TRACK_PARTICLE) == -1 &&
                   !gui.get<bool>(GuiSettingsId::CAMERA_TRACK_MEDIAN);
        });
    transformCat.connect<Float>("Orbit speed [s^-1]", gui, GuiSettingsId::CAMERA_ORBIT);
    transformCat.connect<Vector>("Target [km]", gui, GuiSettingsId::CAMERA_TARGET).setUnits(1.e3_f);
    transformCat.connect<Vector>("Up-direction", gui, GuiSettingsId::CAMERA_UP)
        .setValidator([](const IVirtualEntry::Value& value) {
            const Vector v = value.get<Vector>();
            return v != Vector(0._f);
        });
    transformCat.connect<Float>("Clip near [km]", gui, GuiSettingsId::CAMERA_CLIP_NEAR).setUnits(1.e3_f);
    transformCat.connect<Float>("Clip far [km]", gui, GuiSettingsId::CAMERA_CLIP_FAR).setUnits(1.e3_f);
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
    [](const String& name) { return makeAuto<OrthoCameraJob>(name); },
    "Creates an orthographic camera");

// ----------------------------------------------------------------------------------------------------------
// PerspectiveCameraJob
// ----------------------------------------------------------------------------------------------------------

PerspectiveCameraJob::PerspectiveCameraJob(const String& name)
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
    [](const String& name) { return makeAuto<PerspectiveCameraJob>(name); },
    "Creates a perspective (pinhole) camera.");

// ----------------------------------------------------------------------------------------------------------
// FisheyeCameraJob
// ----------------------------------------------------------------------------------------------------------

FisheyeCameraJob::FisheyeCameraJob(const String& name)
    : ICameraJob(name) {
    gui.set(GuiSettingsId::CAMERA_TYPE, CameraEnum::FISHEYE);
}

VirtualSettings FisheyeCameraJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addResolutionCategory(connector, gui);
    addTransformCategory(connector, gui);
    addTrackingCategory(connector, gui);
    return connector;
}

JobRegistrar sRegisterFisheye(
    "fisheye camera",
    "camera",
    "rendering",
    [](const String& name) { return makeAuto<FisheyeCameraJob>(name); },
    "Creates a fisheye camera.");

// ----------------------------------------------------------------------------------------------------------
// SphericalCameraJob
// ----------------------------------------------------------------------------------------------------------

SphericalCameraJob::SphericalCameraJob(const String& name)
    : ICameraJob(name) {
    gui.set(GuiSettingsId::CAMERA_TYPE, CameraEnum::SPHERICAL);
}

VirtualSettings SphericalCameraJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    addResolutionCategory(connector, gui);
    addTransformCategory(connector, gui);
    addTrackingCategory(connector, gui);
    return connector;
}

JobRegistrar sRegisterSpherical(
    "spherical camera",
    "camera",
    "rendering",
    [](const String& name) { return makeAuto<SphericalCameraJob>(name); },
    L"Creates a spherical 360° camera.");

NAMESPACE_SPH_END
