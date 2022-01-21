#include "gui/jobs/Presets.h"
#include "gui/Factory.h"
#include "gui/objects/CameraJobs.h"
#include "gui/objects/PaletteEntry.h"
#include "gui/objects/RenderJobs.h"
#include "io/FileManager.h"
#include "quantities/Attractor.h"
#include "run/jobs/GeometryJobs.h"
#include "run/jobs/InitialConditionJobs.h"
#include "run/jobs/ParticleJobs.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

static RegisterEnum<GuiId> sPresetsGuiId({
    { GuiId::BLACK_HOLE,
        "black_hole",
        "Preset allows to visualize an accretion disk orbitting a black hole." },
});

}


SharedPtr<JobNode> Presets::make(const ExtId id, UniqueNameManager& nameMgr, const Size particleCnt) {
    switch (GuiId(id)) {
    case GuiId::BLACK_HOLE:
        return makeBlackHole(nameMgr, particleCnt);
    default:
        return make(Id(id), nameMgr, particleCnt);
    }
}

SharedPtr<JobNode> Presets::makeBlackHole(UniqueNameManager& nameMgr, const Size particleCnt) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);
    SharedPtr<JobNode> outerRing = makeNode<CylinderJob>(nameMgr.getName("disk cylinder"));
    VirtualSettings outerRingSettings = outerRing->getSettings();
    outerRingSettings.set("radius", Constants::au / 1.e3_f);
    outerRingSettings.set("height", 0.02_f * Constants::au / 1.e3_f);

    SharedPtr<JobNode> innerRing = makeNode<CylinderJob>(nameMgr.getName("inner gap"));
    VirtualSettings innerRingSettings = innerRing->getSettings();
    innerRingSettings.set("radius", 0.1_f * Constants::au / 1.e3_f);
    innerRingSettings.set("height", 0.02_f * Constants::au / 1.e3_f);

    SharedPtr<JobNode> domain = makeNode<BooleanGeometryJob>(nameMgr.getName("disk shape"));
    outerRing->connect(domain, "operand A");
    innerRing->connect(domain, "operand B");

    SharedPtr<JobNode> disk = makeNode<MonolithicBodyIc>(nameMgr.getName("disk"));
    VirtualSettings diskSettings = disk->getSettings();
    diskSettings.set(BodySettingsId::PARTICLE_COUNT, int(particleCnt));
    diskSettings.set(BodySettingsId::INITIAL_DISTRIBUTION, EnumWrapper(DistributionEnum::STRATIFIED));
    diskSettings.set(BodySettingsId::EOS, EnumWrapper(EosEnum::IDEAL_GAS));
    diskSettings.set(BodySettingsId::RHEOLOGY_YIELDING, EnumWrapper(YieldingEnum::NONE));
    diskSettings.set(BodySettingsId::RHEOLOGY_DAMAGE, EnumWrapper(FractureEnum::NONE));
    diskSettings.set(BodySettingsId::DENSITY, 1._f);
    diskSettings.set(BodySettingsId::ENERGY, 0.01_f);
    diskSettings.set("useShapeSlot", true);
    domain->connect(disk, "shape");

    SharedPtr<JobNode> bh = makeNode<SingleParticleIc>(nameMgr.getName("black hole"));
    VirtualSettings bhSettings = bh->getSettings();
    bhSettings.set("mass", Constants::M_sun / Constants::M_earth);
    bhSettings.set("radius", 2.e7_f);
    bhSettings.set("interaction", EnumWrapper(ParticleInteractionEnum::ABSORB));
    bhSettings.set("albedo", 0._f);

    SharedPtr<JobNode> kepler = makeNode<KeplerianVelocityIc>(nameMgr.getName("set velocities"));
    disk->connect(kepler, "orbiting");
    bh->connect(kepler, "gravity source");

    SharedPtr<JobNode> join = makeNode<JoinParticlesJob>(nameMgr.getName("merge"));
    kepler->connect(join, "particles A");
    bh->connect(join, "particles B");

    SharedPtr<JobNode> camera = makeNode<PerspectiveCameraJob>(nameMgr.getName("camera"));
    VirtualSettings cameraSettings = camera->getSettings();
    cameraSettings.set(GuiSettingsId::CAMERA_POSITION, Vector(3.e8_f, 0._f, 3.e7_f));
    cameraSettings.set(GuiSettingsId::CAMERA_UP, Vector(0._f, 0._f, 1._f));

    SharedPtr<JobNode> render = makeNode<AnimationJob>(nameMgr.getName("render"));
    VirtualSettings renderSettings = render->getSettings();
    renderSettings.set(GuiSettingsId::RENDERER, EnumWrapper(RendererEnum::VOLUME));
    renderSettings.set("quantity", EnumWrapper(RenderColorizerId::VELOCITY));
    renderSettings.set(GuiSettingsId::RAYTRACE_LENSING_MAGNITUDE, 1.e7_f);
    renderSettings.set(GuiSettingsId::VOLUME_EMISSION, 5.e-8_f);
    Palette palette = Factory::getDefaultPalette(Interval(1.e4_f, 1.e5_f));
    palette.setScale(PaletteScale::LOGARITHMIC);
    renderSettings.set("palette", ExtraEntry(makeAuto<PaletteEntry>(palette)));
    join->connect(render, "particles");
    camera->connect(render, "camera");
    return render;
}

NAMESPACE_SPH_END
