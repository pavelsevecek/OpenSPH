#include "gui/problems/Rotation.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "sph/equations/Friction.h"
#include "sph/equations/Potentials.h"
#include "sph/initial/Initial.h"
#include "sph/kernel/Interpolation.h"
#include "sph/solvers/ContinuitySolver.h"
#include "sph/solvers/StaticSolver.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

AsteroidRotation::AsteroidRotation(Controller* model, const Float period)
    : model(model)
    , period(period) {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 100000._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 100._f)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
    settings.saveToFile("code.sph");
}

void AsteroidRotation::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Range(0._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 10000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e5_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true)
        .set(BodySettingsId::SHEAR_MODULUS, 0._f);
    //.set(BodySettingsId::KINEMATIC_VISCOSITY, 1._f);
    bodySettings.saveToFile("target.sph");

    storage = makeShared<Storage>();

    EquationHolder externalForces;
    externalForces += makeTerm<SphericalGravity>(SphericalGravity::Options::ASSUME_HOMOGENEOUS);
    // externalForces += makeTerm<NoninertialForce>(2._f * PI / (3600._f * period) * Vector(0, 0, 1));
    // externalForces += makeTerm<SurfaceNormal>();
    // externalForces += makeTerm<SimpleDamping>();
    solver = makeAuto<ContinuitySolver>(settings, externalForces);

    InitialConditions conds(*storage, *solver, settings);

    StdOutLogger logger;
    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    // const Float P = period * 3600._f;

    conds.addBody(domain1, bodySettings); // , Vector(0._f), Vector(0._f, 0._f, 2._f * PI / P));

    Storage smaller;
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 4000);
    InitialConditions conds2(smaller, settings);
    conds2.addBody(domain1, bodySettings);
    this->setInitialStressTensor(smaller, externalForces);


    logger.write("Particles of target: ", storage->getParticleCnt());

    std::string outputDir = "out/" + settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME);
    output = makeAuto<TextOutput>(
        outputDir, settings.get<std::string>(RunSettingsId::RUN_NAME), TextOutput::Options::SCIENTIFIC);
    output->add(makeAuto<ParticleNumberColumn>());
    output->add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    output->add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
    output->add(makeAuto<SmoothingLengthColumn>());
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    output->add(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    output->add(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));

    logFiles.push(makeAuto<IntegralsLog>("integrals.txt", 1));

    callbacks = makeAuto<GuiCallbacks>(model);
}

void AsteroidRotation::setInitialStressTensor(Storage& smaller, EquationHolder& equations) {

    // Create static solver using different storage (with fewer particles for faster computation)
    StaticSolver staticSolver(settings, equations);
    staticSolver.create(smaller, smaller.getMaterial(0));

    // Solve initial conditions
    /*Statistics stats;
    Outcome result = staticSolver.solve(smaller, stats);
    ASSERT(result);*/

    // Set values of energy and stress in original storage from values computed by static solver
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
    // ArrayView<TracelessTensor> s = storage->getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    // Interpolation interpol(smaller);
    TillotsonEos eos(smaller.getMaterial(0)->getParams());
    const Float rho0 = smaller.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
    Analytic::StaticSphere sphere(5.e3_f, rho0);
    for (Size i = 0; i < r.size(); ++i) {
        // const Float p = sphere.getPressure(getLength(r[i]));
        u[i] = 0._f; // eos.getInternalEnergy(rho0, p);
        // interpol.interpolate<Float>(QuantityId::ENERGY, OrderEnum::ZERO, r[i]);
        /*  s[i] =
              TracelessTensor::null(); //
           interpol.interpolate<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,*/
        // OrderEnum::ZERO, r[i]);
    }
}

void AsteroidRotation::tearDown() {
    Profiler* profiler = Profiler::getInstance();
    profiler->printStatistics(*logger);
}

NAMESPACE_SPH_END
