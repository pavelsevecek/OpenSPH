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

    SharedPtr<WorkerNode> material = makeNode<MaterialWorker>(nameMgr.getName("material"), EMPTY_SETTINGS);

    const Vector domainSize(1.e5_f, 3.e4_f, 1.e5_f);
    SharedPtr<WorkerNode> domain = makeNode<BlockWorker>(nameMgr.getName("target domain"));

    SharedPtr<WorkerNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("useShapeSlot", true);
    targetSettings.set("particles.count", int(particleCnt));
    material->connect(targetIc, "material");
    domain->connect(targetIc, "shape");

    SharedPtr<WorkerNode> impactorIc = makeNode<MonolithicBodyIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);

    const Float impactorRadius = 2._f;
    impactorSettings.set("body.radius", impactorRadius); // D=4km
    impactorSettings.set("particles.count",
        max<int>(50, particleCnt * sphereVolume(impactorRadius) / Box(Vector(0._f), domainSize).volume()));

    material->connect(impactorIc, "material");

    SharedPtr<WorkerNode> merger = makeNode<MergeParticlesWorker>("merger");
    VirtualSettings mergerSettings = merger->getSettings();
    mergerSettings.set("offset", Vector(0._f, 50._f, 0._f));   // 50km
    mergerSettings.set("velocity", Vector(0._f, -5._f, 0._f)); // 5km/s
    targetIc->connect(merger, "particles A");
    impactorIc->connect(merger, "particles B");

    SharedPtr<WorkerNode> cratering = makeNode<SphWorker>(nameMgr.getName("cratering"), EMPTY_SETTINGS);
    VirtualSettings crateringSettings = cratering->getSettings();
    crateringSettings.set("domain.boundary", EnumWrapper(BoundaryEnum::GHOST_PARTICLES));
    Flags<ForceEnum> forces = ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS;
    crateringSettings.set("sph.solver.forces", EnumWrapper(ForceEnum(forces.value())));
    crateringSettings.set("frame.constant_acceleration", Vector(0._f, -1._f, 0._f));

    merger->connect(cratering, "particles");
    domain->connect(cratering, "boundary");

    return cratering;
}

NAMESPACE_SPH_END
