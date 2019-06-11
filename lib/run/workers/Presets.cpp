#include "run/workers/Presets.h"
#include "io/FileSystem.h"
#include "run/workers/GeometryWorkers.h"
#include "run/workers/InitialConditionWorkers.h"
#include "run/workers/IoWorkers.h"
#include "run/workers/MaterialWorkers.h"
#include "run/workers/ParticleWorkers.h"
#include "run/workers/SimulationWorkers.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

SharedPtr<WorkerNode> Presets::makeSimpleCollision(UniqueNameManager& nameMgr, const Size particleCnt) {
    SharedPtr<WorkerNode> targetMaterial =
        makeNode<MaterialWorker>(nameMgr.getName("material"), EMPTY_SETTINGS);
    SharedPtr<WorkerNode> impactorMaterial =
        makeNode<DisableDerivativeCriterionWorker>(nameMgr.getName("optimize impactor"));
    targetMaterial->connect(impactorMaterial, "material");

    SharedPtr<WorkerNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("body.radius", 50._f); // D=100km
    targetSettings.set("particles.count", int(particleCnt));

    SharedPtr<WorkerNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);
    impactorSettings.set("body.radius", 10._f); // D=20km
    targetMaterial->connect(targetIc, "material");
    impactorMaterial->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    CollisionGeometrySettings geometry;
    SharedPtr<WorkerNode> setup = makeNode<CollisionGeometrySetup>(nameMgr.getName("geometry"), geometry);
    targetIc->connect(setup, "target");
    impactorIc->connect(setup, "impactor");

    SharedPtr<WorkerNode> frag = makeNode<SphWorker>(nameMgr.getName("fragmentation"), EMPTY_SETTINGS);
    setup->connect(frag, "particles");

    return frag;
}

SharedPtr<WorkerNode> Presets::makeFragmentationAndReaccumulation(UniqueNameManager& nameMgr,
    const Size particleCnt) {
    makeNode<SphereWorker>("dummy"); /// \todo needed to include geometry in the list, how??

    SharedPtr<WorkerNode> targetMaterial =
        makeNode<MaterialWorker>(nameMgr.getName("material"), EMPTY_SETTINGS);
    SharedPtr<WorkerNode> impactorMaterial =
        makeNode<DisableDerivativeCriterionWorker>(nameMgr.getName("optimize impactor"));
    targetMaterial->connect(impactorMaterial, "material");

    SharedPtr<WorkerNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("body.radius", 50._f); // D=100km
    targetSettings.set("particles.count", int(particleCnt));

    SharedPtr<WorkerNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);
    impactorSettings.set("body.radius", 10._f); // D=20km
    targetMaterial->connect(targetIc, "material");
    impactorMaterial->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    SharedPtr<WorkerNode> stabTarget =
        makeNode<SphStabilizationWorker>(nameMgr.getName("stabilize target"), EMPTY_SETTINGS);
    targetIc->connect(stabTarget, "particles");

    CollisionGeometrySettings geometry;
    SharedPtr<WorkerNode> setup = makeNode<CollisionGeometrySetup>(nameMgr.getName("geometry"), geometry);
    stabTarget->connect(setup, "target");
    impactorIc->connect(setup, "impactor");

    SharedPtr<WorkerNode> frag = makeNode<SphWorker>(nameMgr.getName("fragmentation"), EMPTY_SETTINGS);
    setup->connect(frag, "particles");
    SharedPtr<WorkerNode> handoff = makeNode<SmoothedToSolidHandoff>(nameMgr.getName("handoff"));
    frag->connect(handoff, "particles");

    SharedPtr<WorkerNode> reacc = makeNode<NBodyWorker>(nameMgr.getName("reaccumulation"), EMPTY_SETTINGS);
    handoff->connect(reacc, "particles");

    return reacc;
}

SharedPtr<WorkerNode> Presets::makeCratering(UniqueNameManager& nameMgr, const Size particleCnt) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);

    SharedPtr<WorkerNode> targetMaterial =
        makeNode<MaterialWorker>(nameMgr.getName("material"), EMPTY_SETTINGS);

    const Vector targetSize(1.e5_f, 3.e4_f, 1.e5_f);
    const Vector domainSize(1.e5_f, 1.e5_f, 1.e5_f);
    const Flags<ForceEnum> forces = ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS;

    SharedPtr<WorkerNode> domain = makeNode<BlockWorker>(nameMgr.getName("boundary"));
    VirtualSettings domainSettings = domain->getSettings();
    domainSettings.set("dimensions", domainSize);
    domainSettings.set("center", 0.5_f * (domainSize - targetSize));

    SharedPtr<WorkerNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("particles.count", int(particleCnt));
    targetSettings.set(BodySettingsId::BODY_SHAPE_TYPE, EnumWrapper(DomainEnum::BLOCK));
    targetSettings.set(BodySettingsId::BODY_DIMENSIONS, targetSize);
    targetMaterial->connect(targetIc, "material");

    SharedPtr<WorkerNode> stabilizeTarget =
        makeNode<SphStabilizationWorker>(nameMgr.getName("stabilize target"));
    VirtualSettings stabilizeSettings = stabilizeTarget->getSettings();
    stabilizeSettings.set(RunSettingsId::DOMAIN_BOUNDARY, EnumWrapper(BoundaryEnum::GHOST_PARTICLES));
    stabilizeSettings.set(RunSettingsId::SPH_SOLVER_FORCES, EnumWrapper(ForceEnum(forces.value())));
    stabilizeSettings.set(RunSettingsId::FRAME_CONSTANT_ACCELERATION, Vector(0._f, -10._f, 0._f));
    targetIc->connect(stabilizeTarget, "particles");
    domain->connect(stabilizeTarget, "boundary");

    SharedPtr<WorkerNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);

    const Float impactorRadius = 2._f;
    impactorSettings.set("body.radius", impactorRadius); // D=4km

    SharedPtr<WorkerNode> impactorMaterial =
        makeNode<DisableDerivativeCriterionWorker>(nameMgr.getName("optimize impactor"));
    targetMaterial->connect(impactorMaterial, "material");

    impactorMaterial->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    SharedPtr<WorkerNode> merger = makeNode<MergeParticlesWorker>("merger");
    VirtualSettings mergerSettings = merger->getSettings();
    mergerSettings.set("offset", Vector(0._f, 50._f, 0._f));   // 50km
    mergerSettings.set("velocity", Vector(0._f, -5._f, 0._f)); // 5km/s
    stabilizeTarget->connect(merger, "particles A");
    impactorIc->connect(merger, "particles B");

    SharedPtr<WorkerNode> cratering = makeNode<SphWorker>(nameMgr.getName("cratering"), EMPTY_SETTINGS);
    VirtualSettings crateringSettings = cratering->getSettings();
    crateringSettings.set(RunSettingsId::DOMAIN_BOUNDARY, EnumWrapper(BoundaryEnum::GHOST_PARTICLES));
    crateringSettings.set(RunSettingsId::SPH_SOLVER_FORCES, EnumWrapper(ForceEnum(forces.value())));
    crateringSettings.set(RunSettingsId::FRAME_CONSTANT_ACCELERATION, Vector(0._f, -10._f, 0._f));

    merger->connect(cratering, "particles");
    domain->connect(cratering, "boundary");

    return cratering;
}

NAMESPACE_SPH_END
