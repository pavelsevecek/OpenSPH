#include "gui/rotation/Rotation.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/geometry/Domain.h"
#include "quantities/Iterate.h"
#include "sph/equations/Friction.h"
#include "sph/equations/Potentials.h"
#include "sph/equations/Rotation.h"
#include "sph/initial/Initial.h"
#include "sph/kernel/Interpolation.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/StaticSolver.h"
#include "system/Profiler.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

AsteroidRotation::AsteroidRotation(const RawPtr<Controller> model, const Float period)
    : model(model)
    , period(period) {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-7_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 4.e-6_f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 1._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 100._f)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::UNIFORM_GRID)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_BALSARA, false)
        .set(RunSettingsId::SPH_AV_BALSARA_STORE, false)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 10);
    settings.saveToFile(Path("code.sph"));
}

class DisableDerivativesSolver : public AsymmetricSolver {
private:
    Vector omega;

    Float delta = 0.2_f;

public:
    DisableDerivativesSolver(const RunSettings& settings,
        const Vector& omega,
        const EquationHolder& equations)
        : AsymmetricSolver(settings, getStandardEquations(settings, equations))
        , omega(omega) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        AsymmetricSolver::integrate(storage, stats);
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, 1.e-7f);
        const Float targetTime = 0.01_f;
        for (Size i = 0; i < v.size(); ++i) {
            // gradually decrease the delta
            const Vector rotation = cross(omega, r[i]);
            const Float factor = 1._f + lerp(delta * dt, 0._f, min(t / targetTime, 1._f));
            // we have to dump only the deviation of velocities, not the initial rotation!
            v[i] = (v[i] - rotation) / factor + rotation;
        }
        if (storage.has(QuantityId::DAMAGE) && t < targetTime) {
            ArrayView<Float> D = storage.getValue<Float>(QuantityId::DAMAGE);
            for (Size i = 0; i < D.size(); ++i) {
                D[i] = 0._f;
            }
        }
        if (storage.has(QuantityId::STRESS_REDUCING) && t < targetTime) {
            ArrayView<Float> Y = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
            for (Size i = 0; i < Y.size(); ++i) {
                Y[i] = 1._f;
            }
        }
    }
};

void AsteroidRotation::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 5'000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        //.set(BodySettingsId::ENERGY_MIN, LARGE)
        .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
        .set(BodySettingsId::DAMAGE_MIN, 0.2_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::ELASTIC)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    bodySettings.saveToFile(Path("target.sph"));

    storage = makeShared<Storage>();

    EquationHolder externalForces;
    // externalForces += makeTerm<SolidStressTorque>(settings);
    // externalForces += makeTerm<SphericalGravity>();
    // const Vector omega = 2._f * PI / (3600._f * period) * Vector(0, 1, 0);
    // externalForces += makeTerm<InertialForce>(omega);

    const Vector omega(0._f, 0._f, 32._f);
    solver = makeAuto<DisableDerivativesSolver>(settings, omega, externalForces);

    InitialConditions conds(*solver, settings);

    // Parent body
    AffineMatrix tm = AffineMatrix::rotateX(PI / 2._f);
    TransformedDomain<CylindricalDomain> domain1(tm, Vector(0._f), 0.2_f, 1._f, true); // H = 1m
    conds.addMonolithicBody(*storage, domain1, bodySettings).addRotation(omega, Vector(0._f));
    logger = Factory::getLogger(settings);
    logger->write("Particles of target: ", storage->getParticleCnt());

    if (storage->has(QuantityId::ANGULAR_VELOCITY)) {
        ArrayView<Vector> w = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
        for (Size i = 0; i < w.size(); ++i) {
            w[i] = omega;
        }
    }

    // Auxiliary storage for computing initial pressure and energy
    /*Storage smaller;
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 10000);
    InitialConditions conds2(smaller, settings);
    conds2.addBody(domain1, bodySettings);
    this->setInitialStressTensor(smaller, externalForces);*/

    // Impactor
    /*SphericalDomain domain2(Vector(5097.4509902022_f, 3726.8662269290_f, 0._f), 270.5847632732_f);
    bodySettings.set(BodySettingsId::PARTICLE_COUNT, 100);
    conds.addBody(domain2, bodySettings).addRotation(-omega, BodyView::RotationOrigin::FRAME_ORIGIN);*/


    // Setup output
    Path outputDir = Path("out") / Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
    AutoPtr<TextOutput> textOutput = makeAuto<TextOutput>(
        outputDir, settings.get<std::string>(RunSettingsId::RUN_NAME), TextOutput::Options::SCIENTIFIC);
    textOutput->addColumn(makeAuto<ParticleNumberColumn>());
    textOutput->addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
    textOutput->addColumn(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));
    textOutput->addColumn(makeAuto<SmoothingLengthColumn>());
    textOutput->addColumn(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    textOutput->addColumn(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    textOutput->addColumn(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    textOutput->addColumn(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    textOutput->addColumn(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));
    output = std::move(textOutput);

    triggers.pushBack(makeAuto<IntegralsLog>(Path("integrals.txt"), 1));
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings)));

    callbacks = makeAuto<GuiCallbacks>(*model);
}

void AsteroidRotation::setInitialStressTensor(Storage& smaller, EquationHolder& equations) {

    // Create static solver using different storage (with fewer particles for faster computation)
    StaticSolver staticSolver(settings, equations);
    staticSolver.create(smaller, smaller.getMaterial(0));

    // Solve initial conditions
    Statistics stats;
    Outcome result = staticSolver.solve(smaller, stats);
    ASSERT(result);

    // Set values of energy and stress in original storage from values computed by static solver
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
    ArrayView<TracelessTensor> s = storage->getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    Interpolation interpol(smaller);
    for (Size i = 0; i < r.size(); ++i) {
        u[i] = 0._f; // interpol.interpolate<Float>(QuantityId::ENERGY, OrderEnum::ZERO, r[i]);
        s[i] =
            TracelessTensor::null(); //  interpol.interpolate<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
                                     //  OrderEnum::ZERO, r[i]);
    }
}

void AsteroidRotation::tearDown(const Statistics& UNUSED(stats)) {
    Profiler& profiler = Profiler::getInstance();
    profiler.printStatistics(*logger);
}

NAMESPACE_SPH_END
