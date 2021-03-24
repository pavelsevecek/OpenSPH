#include "run/workers/Presets.h"
#include "io/FileSystem.h"
#include "run/workers/GeometryJobs.h"
#include "run/workers/InitialConditionJobs.h"
#include "run/workers/IoJobs.h"
#include "run/workers/MaterialJobs.h"
#include "run/workers/ParticleJobs.h"
#include "run/workers/SimulationJobs.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

SharedPtr<JobNode> Presets::makeAsteroidCollision(UniqueNameManager& nameMgr, const Size particleCnt) {
    SharedPtr<JobNode> targetMaterial = makeNode<MaterialJob>(nameMgr.getName("material"), EMPTY_SETTINGS);
    SharedPtr<JobNode> impactorMaterial =
        makeNode<DisableDerivativeCriterionJob>(nameMgr.getName("optimize impactor"));
    targetMaterial->connect(impactorMaterial, "material");

    SharedPtr<JobNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("body.radius", 50._f); // D=100km
    targetSettings.set("particles.count", int(particleCnt));

    SharedPtr<JobNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);
    impactorSettings.set("body.radius", 10._f); // D=20km
    targetMaterial->connect(targetIc, "material");
    impactorMaterial->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    CollisionGeometrySettings geometry;
    SharedPtr<JobNode> setup = makeNode<CollisionGeometrySetup>(nameMgr.getName("geometry"), geometry);
    targetIc->connect(setup, "target");
    impactorIc->connect(setup, "impactor");

    SharedPtr<JobNode> frag = makeNode<SphJob>(nameMgr.getName("fragmentation"), EMPTY_SETTINGS);
    setup->connect(frag, "particles");

    return frag;
}

SharedPtr<JobNode> Presets::makeFragmentationAndReaccumulation(UniqueNameManager& nameMgr,
    const Size particleCnt) {
    makeNode<SphereJob>("dummy"); /// \todo needed to include geometry in the list, how??

    SharedPtr<JobNode> targetMaterial = makeNode<MaterialJob>(nameMgr.getName("material"), EMPTY_SETTINGS);
    SharedPtr<JobNode> impactorMaterial =
        makeNode<DisableDerivativeCriterionJob>(nameMgr.getName("optimize impactor"));
    targetMaterial->connect(impactorMaterial, "material");

    SharedPtr<JobNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("body.radius", 50._f); // D=100km
    targetSettings.set("particles.count", int(particleCnt));

    SharedPtr<JobNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);
    impactorSettings.set("body.radius", 10._f); // D=20km
    targetMaterial->connect(targetIc, "material");
    impactorMaterial->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    SharedPtr<JobNode> stabTarget =
        makeNode<SphStabilizationJob>(nameMgr.getName("stabilize target"), EMPTY_SETTINGS);
    targetIc->connect(stabTarget, "particles");

    CollisionGeometrySettings geometry;
    SharedPtr<JobNode> setup = makeNode<CollisionGeometrySetup>(nameMgr.getName("geometry"), geometry);
    stabTarget->connect(setup, "target");
    impactorIc->connect(setup, "impactor");

    SharedPtr<JobNode> frag = makeNode<SphJob>(nameMgr.getName("fragmentation"), EMPTY_SETTINGS);
    setup->connect(frag, "particles");
    SharedPtr<JobNode> handoff = makeNode<SmoothedToSolidHandoff>(nameMgr.getName("handoff"));
    frag->connect(handoff, "particles");

    SharedPtr<JobNode> reacc = makeNode<NBodyJob>(nameMgr.getName("reaccumulation"), EMPTY_SETTINGS);
    handoff->connect(reacc, "particles");

    return reacc;
}

SharedPtr<JobNode> Presets::makeCratering(UniqueNameManager& nameMgr, const Size particleCnt) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);

    SharedPtr<JobNode> targetMaterial = makeNode<MaterialJob>(nameMgr.getName("material"), EMPTY_SETTINGS);

    const Vector targetSize(100._f, 30._f, 100._f);  // in km
    const Vector domainSize(100._f, 100._f, 100._f); // in km
    const Flags<ForceEnum> forces = ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS;
    const Flags<TimeStepCriterionEnum> criteria = TimeStepCriterionEnum::COURANT |
                                                  TimeStepCriterionEnum::DIVERGENCE |
                                                  TimeStepCriterionEnum::DERIVATIVES;

    SharedPtr<JobNode> domain = makeNode<BlockJob>(nameMgr.getName("boundary"));
    VirtualSettings domainSettings = domain->getSettings();
    domainSettings.set("dimensions", domainSize);
    domainSettings.set("center", 0.5_f * (domainSize - targetSize));

    SharedPtr<JobNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("particles.count", int(particleCnt));
    targetSettings.set(BodySettingsId::BODY_SHAPE_TYPE, EnumWrapper(DomainEnum::BLOCK));
    targetSettings.set(BodySettingsId::BODY_DIMENSIONS, targetSize);
    targetMaterial->connect(targetIc, "material");

    SharedPtr<JobNode> stabilizeTarget = makeNode<SphStabilizationJob>(nameMgr.getName("stabilize target"));
    VirtualSettings stabilizeSettings = stabilizeTarget->getSettings();
    stabilizeSettings.set(RunSettingsId::RUN_END_TIME, 40._f);
    stabilizeSettings.set(RunSettingsId::DOMAIN_BOUNDARY, EnumWrapper(BoundaryEnum::GHOST_PARTICLES));
    stabilizeSettings.set(RunSettingsId::SPH_SOLVER_FORCES, EnumWrapper(ForceEnum(forces.value())));
    stabilizeSettings.set(RunSettingsId::FRAME_CONSTANT_ACCELERATION, Vector(0._f, -10._f, 0._f));
    stabilizeSettings.set(
        RunSettingsId::TIMESTEPPING_CRITERION, EnumWrapper(TimeStepCriterionEnum(criteria.value())));
    targetIc->connect(stabilizeTarget, "particles");
    domain->connect(stabilizeTarget, "boundary");

    SharedPtr<JobNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);

    const Float impactorRadius = 2._f;
    impactorSettings.set("body.radius", impactorRadius); // D=4km

    SharedPtr<JobNode> impactorMaterial =
        makeNode<DisableDerivativeCriterionJob>(nameMgr.getName("optimize impactor"));
    targetMaterial->connect(impactorMaterial, "material");

    impactorMaterial->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    SharedPtr<JobNode> merger = makeNode<JoinParticlesJob>("merger");
    VirtualSettings mergerSettings = merger->getSettings();
    mergerSettings.set("offset", Vector(0._f, 50._f, 0._f));   // 50km
    mergerSettings.set("velocity", Vector(0._f, -5._f, 0._f)); // 5km/s
    mergerSettings.set("unique_flags", true);                  // separate the bodies
    stabilizeTarget->connect(merger, "particles A");
    impactorIc->connect(merger, "particles B");

    SharedPtr<JobNode> cratering = makeNode<SphJob>(nameMgr.getName("cratering"), EMPTY_SETTINGS);
    VirtualSettings crateringSettings = cratering->getSettings();
    crateringSettings.set(RunSettingsId::RUN_END_TIME, 60._f);
    crateringSettings.set(RunSettingsId::DOMAIN_BOUNDARY, EnumWrapper(BoundaryEnum::GHOST_PARTICLES));
    crateringSettings.set(RunSettingsId::SPH_SOLVER_FORCES, EnumWrapper(ForceEnum(forces.value())));
    crateringSettings.set(RunSettingsId::FRAME_CONSTANT_ACCELERATION, Vector(0._f, -10._f, 0._f));
    stabilizeSettings.set(
        RunSettingsId::TIMESTEPPING_CRITERION, EnumWrapper(TimeStepCriterionEnum(criteria.value())));

    merger->connect(cratering, "particles");
    domain->connect(cratering, "boundary");

    return cratering;
}

SharedPtr<JobNode> Presets::makeGalaxyCollision(UniqueNameManager& nameMgr, const Size particleCnt) {
    SharedPtr<JobNode> galaxyIc = makeNode<GalaxyIc>(nameMgr.getName("galaxy"));
    VirtualSettings galaxySettings = galaxyIc->getSettings();
    galaxySettings.set(GalaxySettingsId::PARTICLE_RADIUS, 0.01_f);
    galaxySettings.set(GalaxySettingsId::DISK_PARTICLE_COUNT, int(particleCnt) / 2);
    galaxySettings.set(GalaxySettingsId::BULGE_PARTICLE_COUNT, int(particleCnt) / 4);
    galaxySettings.set(GalaxySettingsId::HALO_PARTICLE_COUNT, int(particleCnt) / 4);

    SharedPtr<JobNode> merger = makeNode<JoinParticlesJob>(nameMgr.getName("merge"));
    VirtualSettings mergerSettings = merger->getSettings();
    mergerSettings.set("offset", Vector(0.01_f, 0._f, 0._f));
    mergerSettings.set("velocity", Vector(0._f, 0.0005_f, 0._f));
    mergerSettings.set("com", true);
    mergerSettings.set("unique_flags", true);

    SharedPtr<JobNode> rotator = makeNode<TransformParticlesJob>(nameMgr.getName("rotator"));
    VirtualSettings rotatorSettings = rotator->getSettings();
    rotatorSettings.set("yaw", 30._f); // 30deg

    galaxyIc->connect(merger, "particles A");
    galaxyIc->connect(rotator, "particles");
    rotator->connect(merger, "particles B");


    RunSettings settings = EMPTY_SETTINGS;
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::ELASTIC_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 1._f)
        .set(RunSettingsId::RUN_END_TIME, 30._f)
        .set(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR, 1._f)
        .set(RunSettingsId::GRAVITY_CONSTANT,
            1._f); // should be already provided by GalaxyIc, but doesnt hurt settings explicitly
    SharedPtr<JobNode> run = makeNode<NBodyJob>(nameMgr.getName("N-body simulation"), settings);
    merger->connect(run, "particles");
    return run;
}

SharedPtr<JobNode> Presets::makeAccretionDisk(UniqueNameManager& nameMgr, const Size particleCnt) {
    SharedPtr<JobNode> starIc = makeNode<MonolithicBodyIc>(nameMgr.getName("gas giant"));
    VirtualSettings starSettings = starIc->getSettings();
    starSettings.set(BodySettingsId::PARTICLE_COUNT, int(particleCnt));
    starSettings.set(BodySettingsId::BODY_RADIUS, 200000._f); // km
    starSettings.set(BodySettingsId::DENSITY, 20._f);         // kg/m^3
    starSettings.set(BodySettingsId::EOS, EnumWrapper(EosEnum::IDEAL_GAS));
    starSettings.set(BodySettingsId::RHEOLOGY_YIELDING, EnumWrapper(YieldingEnum::NONE));

    SharedPtr<JobNode> equilibriumIc = makeNode<EquilibriumIc>(nameMgr.getName("hydrostatic equilibrium"));
    starIc->connect(equilibriumIc, "particles");

    SharedPtr<JobNode> nsIc = makeNode<SingleParticleIc>(nameMgr.getName("neutron star"));
    VirtualSettings nsSettings = nsIc->getSettings();
    nsSettings.set("radius", 0.04_f); // R_sun

    SharedPtr<JobNode> join = makeNode<JoinParticlesJob>(nameMgr.getName("geometry setup"));
    VirtualSettings joinSettings = join->getSettings();
    joinSettings.set("offset", Vector(1.e6_f, 0._f, 0._f));
    joinSettings.set("velocity", Vector(0._f, 250._f, 0._f));
    joinSettings.set("com", true);
    equilibriumIc->connect(join, "particles A");
    nsIc->connect(join, "particles B");

    SharedPtr<JobNode> sim = makeNode<SphJob>(nameMgr.getName("accretion"), EMPTY_SETTINGS);
    VirtualSettings simSettings = sim->getSettings();
    simSettings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 50._f);
    simSettings.set(RunSettingsId::RUN_END_TIME, 28800._f);
    const Flags<ForceEnum> forces = ForceEnum::PRESSURE | ForceEnum::SELF_GRAVITY;
    simSettings.set(RunSettingsId::SPH_SOLVER_FORCES, EnumWrapper(ForceEnum(forces.value())));

    join->connect(sim, "particles");
    return sim;
}

static void setPositionAndVelocity(VirtualSettings& settings, const Float radius, const Float longitude) {
    const Float vel = sqrt(Constants::gravity * Constants::M_sun / (radius * Constants::au));
    const Float l = longitude * DEG_TO_RAD + vel / (radius * Constants::au) * 3._f * Constants::year;
    const Vector dir = Vector(cos(l), sin(l), 0._f);
    settings.set("r0", radius * Constants::au / Constants::R_sun * dir);
    settings.set("v0", vel * Constants::year / Constants::R_sun * Vector(-dir[Y], dir[X], 0._f));
}

SharedPtr<JobNode> Presets::makeSolarSystem(UniqueNameManager& nameMgr) {
    // https://aa.quae.nl/en/reken/hemelpositie.html
    SharedPtr<JobNode> join = makeNode<MultiJoinParticlesJob>(nameMgr.getName("create Solar System"));
    join->getSettings().set("slot_cnt", 9);
    SharedPtr<JobNode> sunIc = makeNode<SingleParticleIc>(nameMgr.getName("Sun"));
    VirtualSettings sunSettings = sunIc->getSettings();
    sunSettings.set("mass", 1._f);
    sunSettings.set("radius", 1._f);
    sunSettings.set("flag", 0);
    sunIc->connect(join, "particles 1");

    SharedPtr<JobNode> mercuryIc = makeNode<SingleParticleIc>(nameMgr.getName("Mercury"));
    VirtualSettings mercurySettings = mercuryIc->getSettings();
    mercurySettings.set("mass", 3.285e23_f / Constants::M_sun);
    mercurySettings.set("radius", 2439.7e3_f / Constants::R_sun);
    mercurySettings.set("flag", 1);
    setPositionAndVelocity(mercurySettings, 0.4502213_f, 29.125 + 48.331 + 174.795);
    mercuryIc->connect(join, "particles 2");

    SharedPtr<JobNode> venusIc = makeNode<SingleParticleIc>(nameMgr.getName("Venus"));
    VirtualSettings venusSettings = venusIc->getSettings();
    venusSettings.set("mass", 4.867e24_f / Constants::M_sun);
    venusSettings.set("radius", 6051.8e3_f / Constants::R_sun);
    venusSettings.set("flag", 2);
    setPositionAndVelocity(venusSettings, 0.7263568_f, 54.884 + 76.680 + 50.416);
    venusIc->connect(join, "particles 3");

    SharedPtr<JobNode> earthIc = makeNode<SingleParticleIc>(nameMgr.getName("Earth"));
    VirtualSettings earthSettings = earthIc->getSettings();
    earthSettings.set("mass", Constants::M_earth / Constants::M_sun);
    earthSettings.set("radius", Constants::R_earth / Constants::R_sun);
    earthSettings.set("flag", 3);
    setPositionAndVelocity(earthSettings, 1._f, 288.064 + 174.873 + 357.529);
    earthIc->connect(join, "particles 4");

    SharedPtr<JobNode> marsIc = makeNode<SingleParticleIc>(nameMgr.getName("Mars"));
    VirtualSettings marsSettings = marsIc->getSettings();
    marsSettings.set("mass", 6.39e23_f / Constants::M_sun);
    marsSettings.set("radius", 3389.5e3_f / Constants::R_sun);
    marsSettings.set("flag", 4);
    setPositionAndVelocity(marsSettings, 1.6086343_f, 286.502 + 49.558 + 19.373);
    marsIc->connect(join, "particles 5");

    SharedPtr<JobNode> jupiterIc = makeNode<SingleParticleIc>(nameMgr.getName("Jupiter"));
    VirtualSettings jupiterSettings = jupiterIc->getSettings();
    jupiterSettings.set("mass", 1.898e27_f / Constants::M_sun);
    jupiterSettings.set("radius", 69911.e3_f / Constants::R_sun);
    jupiterSettings.set("flag", 5);
    setPositionAndVelocity(jupiterSettings, 5.0684375_f, 273.867 + 100.464 + 20.020);
    jupiterIc->connect(join, "particles 6");

    SharedPtr<JobNode> saturnIc = makeNode<SingleParticleIc>(nameMgr.getName("Saturn"));
    VirtualSettings saturnSettings = saturnIc->getSettings();
    saturnSettings.set("mass", 5.683e26_f / Constants::M_sun);
    saturnSettings.set("radius", 58232.e3_f / Constants::R_sun);
    saturnSettings.set("flag", 6);
    setPositionAndVelocity(saturnSettings, 9.9734145_f, 339.391 + 113.666 + 317.021);
    saturnIc->connect(join, "particles 7");

    SharedPtr<JobNode> uranusIc = makeNode<SingleParticleIc>(nameMgr.getName("Uranus"));
    VirtualSettings uranusSettings = uranusIc->getSettings();
    uranusSettings.set("mass", 8.681e25_f / Constants::M_sun);
    uranusSettings.set("radius", 25362e3_f / Constants::R_sun);
    uranusSettings.set("flag", 7);
    setPositionAndVelocity(uranusSettings, 19.7612021_f, 98.999 + 74.006 + 141.050);
    uranusIc->connect(join, "particles 8");

    SharedPtr<JobNode> neptuneIc = makeNode<SingleParticleIc>(nameMgr.getName("Neptune"));
    VirtualSettings neptuneSettings = neptuneIc->getSettings();
    neptuneSettings.set("mass", 1.024e26_f / Constants::M_sun);
    neptuneSettings.set("radius", 24622e3_f / Constants::R_sun);
    neptuneSettings.set("flag", 8);
    setPositionAndVelocity(neptuneSettings, 29.9254883_f, 276.340 + 131.784 + 256.225);
    neptuneIc->connect(join, "particles 9");

    SharedPtr<JobNode> sim = makeNode<NBodyJob>(nameMgr.getName("orbital simulation"), EMPTY_SETTINGS);
    join->connect(sim, "particles");
    VirtualSettings simSettings = sim->getSettings();
    simSettings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 3600._f);
    simSettings.set(RunSettingsId::RUN_END_TIME, Constants::year * 100._f);
    simSettings.set(RunSettingsId::RUN_LOGGER_VERBOSITY, 0);
    return sim;
}

NAMESPACE_SPH_END
