#include "run/workers/Presets.h"
#include "io/FileSystem.h"
#include "run/workers/GeometryWorkers.h"
#include "run/workers/InitialConditionWorkers.h"
#include "run/workers/IoWorkers.h"
#include "run/workers/MaterialWorkers.h"
#include "run/workers/ParticleWorkers.h"
#include "run/workers/SimulationWorkers.h"

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

NAMESPACE_SPH_END
