#include "gui/rotation/Rotation.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "quantities/Iterate.h"
#include "sph/equations/Friction.h"
#include "sph/equations/Potentials.h"
#include "sph/initial/Initial.h"
#include "sph/kernel/Interpolation.h"
#include "sph/solvers/ContinuitySolver.h"
#include "sph/solvers/StaticSolver.h"
#include "system/Profiler.h"

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

AsteroidRotation::AsteroidRotation(Controller* model, const Float period)
    : model(model)
    , period(period) {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.01_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 100000._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 100._f)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
    settings.saveToFile(Path("code.sph"));
}

class DisableDerivativesSolver : public ContinuitySolver {
private:
    Float delta = 0.0_f;

public:
    DisableDerivativesSolver(const RunSettings& settings, const EquationHolder& equations)
        : ContinuitySolver(settings, equations) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        ContinuitySolver::integrate(storage, stats);
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        for (Size i = 0; i < v.size(); ++i) {
            // gradually decrease the delta
            const Float t = stats.get<Float>(StatisticsId::TOTAL_TIME);
            v[i] /= 1._f + lerp(delta, 0._f, min(t / 10._f, 1._f));
        }
    }
};

void AsteroidRotation::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Range(0._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 100000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e5_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, true);
    bodySettings.saveToFile(Path("target.sph"));

    storage = makeShared<Storage>();

    EquationHolder externalForces;
    externalForces += makeTerm<SphericalGravity>(SphericalGravity::Options::ASSUME_HOMOGENEOUS);
    const Vector omega = 2._f * PI / (3600._f * period) * Vector(0, 1, 0);
    externalForces += makeTerm<NoninertialForce>(omega);

    solver = makeAuto<DisableDerivativesSolver>(settings, externalForces);

    InitialConditions conds(*storage, *solver, settings);

    // Parent body
    SphericalDomain domain1(Vector(0._f), 5e3_f); // D = 10km
    conds.addBody(domain1, bodySettings);
    logger = Factory::getLogger(settings);
    logger->write("Particles of target: ", storage->getParticleCnt());

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
    textOutput->add(makeAuto<ParticleNumberColumn>());
    textOutput->add(makeAuto<ValueColumn<Vector>>(QuantityId::POSITIONS));
    textOutput->add(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITIONS));
    textOutput->add(makeAuto<SmoothingLengthColumn>());
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    textOutput->add(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    textOutput->add(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));
    output = std::move(textOutput);

    logFiles.push(makeAuto<IntegralsLog>(Path("integrals.txt"), 1));

    callbacks = makeAuto<GuiCallbacks>(model);
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
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITIONS);
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

void AsteroidRotation::tearDown() {
    Profiler* profiler = Profiler::getInstance();
    profiler->printStatistics(*logger);
}

NAMESPACE_SPH_END
