#include "run/jobs/Presets.h"
#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "run/jobs/GeometryJobs.h"
#include "run/jobs/InitialConditionJobs.h"
#include "run/jobs/IoJobs.h"
#include "run/jobs/MaterialJobs.h"
#include "run/jobs/ParticleJobs.h"
#include "run/jobs/SimulationJobs.h"
#include "sph/Materials.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

static RegisterEnum<Id> sPresetsId({
    { Id::COLLISION, "collision", "Simple simulation of a two-body collision." },
    { Id::FRAGMENTATION_REACCUMULATION,
        "fragmentation_and_reaccumulation",
        "SPH simulation of an impact and fragmentation followed by an N-body simulation of gravitational "
        "reaccumulation of fragments." },
    { Id::CRATERING,
        "cratering",
        "Meteoroid impact to a horizontal surface enclosed by boundary conditions." },
    { Id::PLANETESIMAL_MERGING,
        "planetesimal_merging",
        "Two equal-sized planetesimals with iron core colliding and merging." },
    { Id::GALAXY_COLLISION, "galaxy_collision", "Simulation of two interacting galaxies." },
    { Id::ACCRETION_DISK,
        "accretion_disk",
        "Gas giant orbiting a neutron star and creating an accretion disk." },
    { Id::SOLAR_SYSTEM,
        "solar_system",
        "N-body simulation of the Sun and eight planets of our Solar System." },
});

}

SharedPtr<JobNode> Presets::make(const Id id, UniqueNameManager& nameMgr, const Size particleCnt) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);
    switch (id) {
    case Id::COLLISION:
        return makeAsteroidCollision(nameMgr, particleCnt);
    case Id::FRAGMENTATION_REACCUMULATION:
        return makeFragmentationAndReaccumulation(nameMgr, particleCnt);
    case Id::CRATERING:
        return makeCratering(nameMgr, particleCnt);
    case Id::PLANETESIMAL_MERGING:
        return makePlanetesimalMerging(nameMgr, particleCnt);
    case Id::GALAXY_COLLISION:
        return makeGalaxyCollision(nameMgr, particleCnt);
    case Id::ACCRETION_DISK:
        return makeAccretionDisk(nameMgr, particleCnt);
    case Id::SOLAR_SYSTEM:
        return makeSolarSystem(nameMgr);
    default:
        NOT_IMPLEMENTED;
    }
}

SharedPtr<JobNode> Presets::makeAsteroidCollision(UniqueNameManager& nameMgr, const Size particleCnt) {
    BodySettings materialOverrides = EMPTY_SETTINGS;
    materialOverrides.set(BodySettingsId::ENERGY, 0._f);

    SharedPtr<JobNode> targetIc = makeNode<MonolithicBodyIc>(nameMgr.getName("target body"));
    VirtualSettings targetSettings = targetIc->getSettings();
    targetSettings.set("useMaterialSlot", true);
    targetSettings.set("body.radius", 50._f); // D=100km
    targetSettings.set("particles.count", int(particleCnt));

    SharedPtr<JobNode> impactorIc = makeNode<ImpactorIc>(nameMgr.getName("impactor body"));
    VirtualSettings impactorSettings = impactorIc->getSettings();
    impactorSettings.set("useMaterialSlot", true);
    impactorSettings.set("body.radius", 10._f); // D=20km

    SharedPtr<JobNode> material = makeNode<MaterialJob>(nameMgr.getName("material"), materialOverrides);
    material->connect(targetIc, "material");
    material->connect(impactorIc, "material");
    targetIc->connect(impactorIc, "target");

    CollisionGeometrySettings geometry;
    SharedPtr<JobNode> setup = makeNode<CollisionGeometrySetup>(nameMgr.getName("geometry"), geometry);
    targetIc->connect(setup, "target");
    impactorIc->connect(setup, "impactor");

    RunSettings settings(EMPTY_SETTINGS);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION,
        TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::DIVERGENCE);
    SharedPtr<JobNode> frag = makeNode<SphJob>(nameMgr.getName("fragmentation"), settings);
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

    RunSettings settings(EMPTY_SETTINGS);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION,
        TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::DIVERGENCE);
    SharedPtr<JobNode> frag = makeNode<SphJob>(nameMgr.getName("fragmentation"), settings);
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

SharedPtr<JobNode> Presets::makePlanetesimalMerging(UniqueNameManager& nameMgr, const Size particleCnt) {
    SharedPtr<JobNode> planetesimal = makeNode<DifferentiatedBodyIc>(nameMgr.getName("planetesimal"));
    VirtualSettings planetSettings = planetesimal->getSettings();
    planetSettings.set(BodySettingsId::PARTICLE_COUNT, int(particleCnt));

    SharedPtr<JobNode> olivine =
        makeNode<MaterialJob>(nameMgr.getName("olivine"), getMaterial(MaterialEnum::OLIVINE)->getParams());
    olivine->getSettings().set(BodySettingsId::RHEOLOGY_YIELDING, EnumWrapper(YieldingEnum::DUST));
    SharedPtr<JobNode> iron =
        makeNode<MaterialJob>(nameMgr.getName("iron"), getMaterial(MaterialEnum::IRON)->getParams());
    iron->getSettings().set(BodySettingsId::RHEOLOGY_YIELDING, EnumWrapper(YieldingEnum::DUST));

    SharedPtr<JobNode> surface = makeNode<SphereJob>("surface sphere");
    surface->getSettings().set("radius", 1500._f); // km

    SharedPtr<JobNode> core = makeNode<SphereJob>("core sphere");
    core->getSettings().set("radius", 750._f); // km

    surface->connect(planetesimal, "base shape");
    olivine->connect(planetesimal, "base material");

    core->connect(planetesimal, "shape 1");
    iron->connect(planetesimal, "material 1");

    SharedPtr<JobNode> equilibrium = makeNode<EquilibriumIc>("hydrostatic equilibrium");
    planetesimal->connect(equilibrium, "particles");

    SharedPtr<JobNode> stab = makeNode<SphStabilizationJob>(nameMgr.getName("stabilize"));
    VirtualSettings stabSettings = stab->getSettings();
    stabSettings.set(RunSettingsId::RUN_END_TIME, 1000._f);
    const Flags<TimeStepCriterionEnum> criteria =
        TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::DIVERGENCE;
    stabSettings.set(
        RunSettingsId::TIMESTEPPING_CRITERION, EnumWrapper(TimeStepCriterionEnum(criteria.value())));
    equilibrium->connect(stab, "particles");

    SharedPtr<JobNode> merger = makeNode<JoinParticlesJob>(nameMgr.getName("merge"));
    VirtualSettings mergerSettings = merger->getSettings();
    mergerSettings.set("offset", Vector(5000._f, 1500._f, 0._f));
    mergerSettings.set("velocity", Vector(-2.5_f, 0._f, 0._f));
    mergerSettings.set("com", true);
    mergerSettings.set("unique_flags", true);

    stab->connect(merger, "particles A");
    stab->connect(merger, "particles B");

    SharedPtr<JobNode> sim = makeNode<SphJob>(nameMgr.getName("impact simulation"));
    VirtualSettings simSettings = sim->getSettings();
    simSettings.set(RunSettingsId::RUN_END_TIME, 15000._f);
    simSettings.set(
        RunSettingsId::TIMESTEPPING_CRITERION, EnumWrapper(TimeStepCriterionEnum(criteria.value())));
    merger->connect(sim, "particles");

    return sim;
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
    settings.set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::NONE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::NONE)
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
    nsSettings.set("radius", 0.04_f * Constants::R_sun / 1.e3_f); // R_sun -> km
    nsSettings.set("mass", Constants::M_sun / Constants::M_earth);

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

/*static void setPositionAndVelocity(VirtualSettings& settings, const Float radius, const Float longitude) {
    const Float vel = sqrt(Constants::gravity * Constants::M_sun / (radius * Constants::au));
    const Float l = longitude * DEG_TO_RAD + vel / (radius * Constants::au) * 3._f * Constants::year;
    const Vector dir = Vector(cos(l), sin(l), 0._f);
    settings.set("r0", radius * Constants::au / Constants::R_sun * dir);
    settings.set("v0", vel * Constants::year / Constants::R_sun * Vector(-dir[Y], dir[X], 0._f));
}*/

// Ephemeris at epoch K20CH
StaticArray<Vector, 8> POSITIONS = {
    Vector(-8.928734732644719e-2, -4.521325453222446e-1, -2.975182102295437e-2),
    Vector(-6.441236962991079e-1, -3.314276376252449e-1, 3.226254091757653e-2),
    Vector(7.549485485202402e-2, 9.867433026966754e-1, 5.784067376482213e-5),
    Vector(7.910150731229694e-1, 1.264441562325783e0, 6.907352037330410e-3),
    Vector(2.944626847316137e0, -4.154019886078014e0, -4.864670549497938e-2),
    Vector(5.418279655781740e0, -8.381621945307499e0, -6.997483972704878e-2),
    Vector(1.538095560420909e1, 1.242451334531269e1, -1.531172265021136e-1),
    Vector(2.944592389131131e1, -5.272456275707134e0, -5.700349742119496e-1),
};
StaticArray<Vector, 8> VELOCITIES = {
    Vector(2.203479749687471e-2, -3.580432587616751e-3, -2.313817126869404e-3),
    Vector(9.315663381232362e-3, -1.797621186456914e-2, -7.843669810993209e-4),
    Vector(-1.743861165458079e-2, 1.361273813138455e-3, 7.711170058594351e-7),
    Vector(-1.129568889262675e-2, 8.676624992146748e-3, 4.590908874884053e-4),
    Vector(6.063442036100106e-3, 4.721743950691111e-3, -1.552093577175655e-4),
    Vector(4.374201049769948e-3, 3.015429419149447e-3, -2.269352878444703e-4),
    Vector(-2.500397529553041e-3, 2.876319493906491e-3, 4.298287724104297e-5),
    Vector(5.329085656143531e-4, 3.108706732834171e-3, -7.657253104891884e-5),
};

static void setPositionAndVelocity(VirtualSettings& settings, const Size idx) {
    const Vector r = POSITIONS[idx] * Constants::au / 1.e3_f;
    const Vector v = VELOCITIES[idx] * (Constants::au / Constants::day) / 1.e3_f;
    settings.set("r0", r);
    settings.set("v0", v);
}

SharedPtr<JobNode> Presets::makeSolarSystem(UniqueNameManager& nameMgr, const Size particleCnt) {
    // https://aa.quae.nl/en/reken/hemelpositie.html
    SharedPtr<JobNode> join = makeNode<MultiJoinParticlesJob>(nameMgr.getName("create Solar System"));
    join->getSettings().set("slot_cnt", 10);
    SharedPtr<JobNode> sunIc = makeNode<SingleParticleIc>(nameMgr.getName("Sun"));
    VirtualSettings sunSettings = sunIc->getSettings();
    sunSettings.set("mass", Constants::M_sun / Constants::M_earth);
    sunSettings.set("radius", Constants::R_sun / 1.e3_f);
    sunIc->connect(join, "particles 1");

    SharedPtr<JobNode> mercuryIc = makeNode<SingleParticleIc>(nameMgr.getName("Mercury"));
    VirtualSettings mercurySettings = mercuryIc->getSettings();
    mercurySettings.set("mass", 3.285e23_f / Constants::M_earth);
    mercurySettings.set("radius", 2439.7e3_f / 1.e3_f);
    setPositionAndVelocity(mercurySettings, 0);
    // setPositionAndVelocity(mercurySettings, 0.4502213_f, 29.125 + 48.331 + 174.795);
    mercuryIc->connect(join, "particles 2");

    SharedPtr<JobNode> venusIc = makeNode<SingleParticleIc>(nameMgr.getName("Venus"));
    VirtualSettings venusSettings = venusIc->getSettings();
    venusSettings.set("mass", 4.867e24_f / Constants::M_earth);
    venusSettings.set("radius", 6051.8e3_f / 1.e3_f);
    setPositionAndVelocity(venusSettings, 1);
    // setPositionAndVelocity(venusSettings, 0.7263568_f, 54.884 + 76.680 + 50.416);
    venusIc->connect(join, "particles 3");

    SharedPtr<JobNode> earthIc = makeNode<SingleParticleIc>(nameMgr.getName("Earth"));
    VirtualSettings earthSettings = earthIc->getSettings();
    earthSettings.set("mass", Constants::M_earth / Constants::M_earth);
    earthSettings.set("radius", Constants::R_earth / 1.e3_f);
    setPositionAndVelocity(earthSettings, 2);
    // setPositionAndVelocity(earthSettings, 1._f, 288.064 + 174.873 + 357.529);
    earthIc->connect(join, "particles 4");

    SharedPtr<JobNode> marsIc = makeNode<SingleParticleIc>(nameMgr.getName("Mars"));
    VirtualSettings marsSettings = marsIc->getSettings();
    marsSettings.set("mass", 6.39e23_f / Constants::M_earth);
    marsSettings.set("radius", 3389.5e3_f / 1.e3_f);
    setPositionAndVelocity(marsSettings, 3);
    // setPositionAndVelocity(marsSettings, 1.6086343_f, 286.502 + 49.558 + 19.373);
    marsIc->connect(join, "particles 5");

    SharedPtr<JobNode> beltIc = makeNode<NBodyIc>(nameMgr.getName("Main belt"));
    VirtualSettings beltSettings = beltIc->getSettings();
    beltSettings.set(NBodySettingsId::PARTICLE_COUNT, int(particleCnt));
    beltSettings.set(NBodySettingsId::TOTAL_MASS, 4.008e-4_f); // M_earth
    beltSettings.set(NBodySettingsId::RADIAL_PROFILE, 0.5_f);
    beltSettings.set(NBodySettingsId::MIN_SEPARATION, 10000._f);
    beltSettings.set("min_size", 1.e4_f); // m
    beltSettings.set("max_size", 4.e5_f); // m
    beltSettings.set(NBodySettingsId::VELOCITY_DISPERSION, 0._f);
    beltSettings.set(NBodySettingsId::VELOCITY_MULTIPLIER, 0._f);

    SharedPtr<JobNode> beltDomain = makeNode<BooleanGeometryJob>(nameMgr.getName("Belt domain"));
    SharedPtr<JobNode> outerBeltDomain = makeNode<CylinderJob>(nameMgr.getName("Outer limit"));
    VirtualSettings outerBeltSettings = outerBeltDomain->getSettings();
    outerBeltSettings.set("radius", 3.3_f * Constants::au / 1.e3_f); // km
    outerBeltSettings.set("height", 0.5_f * Constants::au / 1.e3_f); // km
    SharedPtr<JobNode> innerBeltDomain = makeNode<CylinderJob>(nameMgr.getName("Inner limit"));
    VirtualSettings innerBeltSettings = innerBeltDomain->getSettings();
    innerBeltSettings.set("radius", 1.8_f * Constants::au / 1.e3_f); // km
    innerBeltSettings.set("height", 0.5_f * Constants::au / 1.e3_f); // km

    outerBeltDomain->connect(beltDomain, "operand A");
    innerBeltDomain->connect(beltDomain, "operand B");

    beltDomain->connect(beltIc, "domain");

    SharedPtr<JobNode> beltVelocities =
        makeNode<KeplerianVelocityIc>(nameMgr.getName("Set orbital velocities"));
    beltIc->connect(beltVelocities, "orbiting");
    sunIc->connect(beltVelocities, "gravity source");

    beltVelocities->connect(join, "particles 6");

    SharedPtr<JobNode> jupiterIc = makeNode<SingleParticleIc>(nameMgr.getName("Jupiter"));
    VirtualSettings jupiterSettings = jupiterIc->getSettings();
    jupiterSettings.set("mass", 1.898e27_f / Constants::M_earth);
    jupiterSettings.set("radius", 69911.e3_f / 1.e3_f);
    setPositionAndVelocity(jupiterSettings, 4);
    // setPositionAndVelocity(jupiterSettings, 5.0684375_f, 273.867 + 100.464 + 20.020);
    jupiterIc->connect(join, "particles 7");

    SharedPtr<JobNode> saturnIc = makeNode<SingleParticleIc>(nameMgr.getName("Saturn"));
    VirtualSettings saturnSettings = saturnIc->getSettings();
    saturnSettings.set("mass", 5.683e26_f / Constants::M_earth);
    saturnSettings.set("radius", 58232.e3_f / 1.e3_f);
    setPositionAndVelocity(saturnSettings, 5);
    // setPositionAndVelocity(saturnSettings, 9.9734145_f, 339.391 + 113.666 + 317.021);
    saturnIc->connect(join, "particles 8");

    SharedPtr<JobNode> uranusIc = makeNode<SingleParticleIc>(nameMgr.getName("Uranus"));
    VirtualSettings uranusSettings = uranusIc->getSettings();
    uranusSettings.set("mass", 8.681e25_f / Constants::M_earth);
    uranusSettings.set("radius", 25362e3_f / 1.e3_f);
    setPositionAndVelocity(uranusSettings, 6);
    // setPositionAndVelocity(uranusSettings, 19.7612021_f, 98.999 + 74.006 + 141.050);
    uranusIc->connect(join, "particles 9");

    SharedPtr<JobNode> neptuneIc = makeNode<SingleParticleIc>(nameMgr.getName("Neptune"));
    VirtualSettings neptuneSettings = neptuneIc->getSettings();
    neptuneSettings.set("mass", 1.024e26_f / Constants::M_earth);
    neptuneSettings.set("radius", 24622e3_f / 1.e3_f);
    setPositionAndVelocity(neptuneSettings, 7);
    // setPositionAndVelocity(neptuneSettings, 29.9254883_f, 276.340 + 131.784 + 256.225);
    neptuneIc->connect(join, "particles 10");

    SharedPtr<JobNode> sim = makeNode<NBodyJob>(nameMgr.getName("orbital simulation"), EMPTY_SETTINGS);
    join->connect(sim, "particles");
    VirtualSettings simSettings = sim->getSettings();
    simSettings.set(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR, 10._f);
    simSettings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 36000._f);
    simSettings.set(RunSettingsId::RUN_END_TIME, Constants::year * 100._f);
    simSettings.set(RunSettingsId::RUN_LOGGER_VERBOSITY, 0);
    return sim;
}

NAMESPACE_SPH_END
