#include "gui/nbody/NBody.h"
#include "gravity/NBodySolver.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "objects/geometry/Domain.h"
#include "quantities/IMaterial.h"
#include "sph/initial/Distribution.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

NBody::NBody() {
    settings.set(RunSettingsId::RUN_NAME, std::string("NBody"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e4_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 1.e-3_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e10_f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e10_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::POINT_PARTICLES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.5_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
}

void NBody::setUp() {
    // we don't need any material, so just pass some dummy
    storage = makeShared<Storage>(makeAuto<NullMaterial>(EMPTY_SETTINGS));

    solver = makeAuto<NBodySolver>(settings);


    /*    RandomDistribution rndDist(124);
        Array<Vector> dist = rndDist.generate(20, SphericalDomain(Vector(0._f), Constants::au));
        storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(dist));
        ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            r[i][H] = 0.02_f * Constants::au;
        }

        Array<Vector>& v = storage->getDt<Vector>(QuantityId::POSITION);
        v = rndDist.generate(20, SphericalDomain(Vector(0._f), 5.e4_f));*/

    Array<Vector> dist{ Vector(-0.5_f * Constants::au, 0._f, 0._f, 0.02_f * Constants::au),
        Vector(0.5_f * Constants::au, 0._f, 0._f, 0.02_f * Constants::au) };
    storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(dist));


    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, Constants::M_sun);

    // create the solver quantities
    solver->create(*storage, storage->getMaterial(0));

    ASSERT(storage->isValid());

    callbacks = makeAuto<GuiCallbacks>(*controller);
}

void NBody::tearDown() {}


NAMESPACE_SPH_END
