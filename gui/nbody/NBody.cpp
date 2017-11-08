#include "gui/nbody/NBody.h"
#include "gravity/NBodySolver.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "quantities/IMaterial.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

NBody::NBody() {
    settings.set(RunSettingsId::RUN_NAME, std::string("NBody"))
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e3_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 1.e-3_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e8_f))
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

    // add two particles
    const Float dx = 0.25_f * Constants::au;
    Quantity& quantity = storage->insert<Vector>(
        QuantityId::POSITIONS, OrderEnum::SECOND, { Vector(-dx, 0._f, 0._f), Vector(dx, 0._f, 0._f) });
    ArrayView<Vector> r = quantity.getValue<Vector>();
    r[0][H] = 0.01_f * Constants::au;
    r[1][H] = 0.05_f * Constants::au;

    ArrayView<Vector> v = quantity.getDt<Vector>();
    v[0] = Vector(0._f, 1.e4_f, 0._f);
    v[1] = Vector(0._f, -1.e4_f, 0._f);

    storage->insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, Constants::M_sun);

    // create the solver quantities
    solver->create(*storage, storage->getMaterial(0));

    callbacks = makeAuto<GuiCallbacks>(controller);
}

void NBody::tearDown() {}


NAMESPACE_SPH_END
