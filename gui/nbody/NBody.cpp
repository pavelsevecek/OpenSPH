#include "gui/nbody/NBody.h"
#include "gravity/NBodySolver.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/LogFile.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "sph/initial/Distribution.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

NBody::NBody() {
    settings.set(RunSettingsId::RUN_NAME, std::string("NBody"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.1_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e10_f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e10_f)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::POINT_PARTICLES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.5_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.8_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 1._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
}

void NBody::setUp() {
    // we don't need any material, so just pass some dummy
    storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));

    solver = makeAuto<NBodySolver>(settings);


    RandomDistribution rndDist(makeRng<HaltonQrng>());
    const Size particleCnt = 150;

    Array<Vector> dist = rndDist.generate(particleCnt, SphericalDomain(Vector(0._f), 2._f * Constants::au));
    storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(dist));
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);

    Array<Vector>& v = storage->getDt<Vector>(QuantityId::POSITION);

    for (Size i = 0; i < r.size(); ++i) {
        r[i][Z] = 0._f;
        v[i] = 2.e5_f *
               (cross(r[i] * pow<2>(Constants::au) / pow<3>(getLength(r[i])), Vector(0._f, 0._f, 1._f)));
        v[i][Z] = 0._f;
        r[i][H] = 0.02_f * Constants::au;
    }


    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, Constants::M_sun);

    // create the solver quantities
    solver->create(*storage, storage->getMaterial(0));

    ASSERT(storage->isValid());

    callbacks = makeAuto<GuiCallbacks>(*controller);

    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));
}

void NBody::tearDown() {}


NAMESPACE_SPH_END