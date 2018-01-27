#include "gui/nbody/NBody.h"
#include "gravity/NBodySolver.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/LogFile.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Platform.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

NBody::NBody() {
    settings.set(RunSettingsId::RUN_NAME, std::string("NBody"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-1_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 1._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e7_f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e10_f)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::POINT_PARTICLES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.5_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::MERGE_OR_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.1_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
}

void NBody::setUp() {
    // we don't need any material, so just pass some dummy
    storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    solver = makeAuto<NBodySolver>(settings);

    RandomDistribution rndDist(makeRng<UniformRng>());
    const Size particleCnt = 100'000;

    Array<Vector> dist = rndDist.generate(particleCnt, SphericalDomain(Vector(0._f), 1.e3_f));
    // dist.push(Vector(0._f));
    storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(dist));
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);

    Array<Vector>& v = storage->getDt<Vector>(QuantityId::POSITION);

    for (Size i = 0; i < r.size(); ++i) {
        r[i][Z] = 0._f;
        r[i][H] = 0.3_f;
    }

    spaceParticles(r, 2._f);
    v = rndDist.generate(particleCnt, SphericalDomain(Vector(0._f), 3.e-2_f));
    for (Size i = 0; i < r.size(); ++i) {
        // const Float kepler = sqrt(Constants::gravity * Constants::M_sun / getLength(r[i]));
        v[i] += cross(r[i] / 1.e3_f, Vector(0._f, 0._f, 1._f)) * 8.e-2_f;
        v[i][Z] = 0._f;
        v[i][H] = 0._f;
    }


    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 2.e5_f);
    // ArrayView<Float> m = storage->getValue<Float>(QuantityId::MASS);
    // m[0] = 0.1_f * Constants::M_sun;
    // m[m.size() - 1] = Constants::M_sun;

    // create the solver quantities
    solver->create(*storage, storage->getMaterial(0));

    ASSERT(storage->isValid());

    callbacks = makeAuto<GuiCallbacks>(*controller);

    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
}

void NBody::tearDown() {
    showNotification("NBody", "Run finished");
}


NAMESPACE_SPH_END
