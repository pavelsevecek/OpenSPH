#include "gui/problems/Rotation.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/Column.h"
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

NAMESPACE_SPH_BEGIN

AsteroidRotation::AsteroidRotation(Controller* model, const Float period)
    : model(model)
    , period(period) {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::EULER_EXPLICIT)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 0.1_f)
        .set(RunSettingsId::RUN_TIME_RANGE, Range(0._f, 100000._f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 100._f)
        .set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::VOXEL)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_ALPHA, 1.5_f)
        .set(RunSettingsId::SPH_AV_BETA, 3._f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100);
    settings.saveToFile("code.sph");
}

class DisableDerivativesSolver : public ContinuitySolver {
private:
    Float delta = 0.3_f;

public:
    DisableDerivativesSolver(const RunSettings& settings, const EquationHolder& equations)
        : ContinuitySolver(settings, equations) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        ContinuitySolver::integrate(storage, stats);
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITIONS);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] /= 1._f + delta;
        }
        /*    iterate<VisitorEnum::FIRST_ORDER>(storage, [this](const QuantityId id, auto& UNUSED(v), auto&
           dv) {
                using Type = typename std::decay_t<decltype(dv)>::Type;
                if (id == QuantityId::ENERGY) {
                    for (Size i = 0; i < dv.size(); ++i) {
                        dv[i] /= 1._f + delta;
                    }
                } else {
                    for (Size i = 0; i < dv.size(); ++i) {
                        dv[i] = Type(0._f);
                    }
                }
            });
            iterate<VisitorEnum::SECOND_ORDER>(
                storage, [this](const QuantityId id, auto& UNUSED(v), auto& dv, auto& d2v) {
                    using Type = typename std::decay_t<decltype(dv)>::Type;
                    if (id == QuantityId::POSITIONS) {
                        for (Size i = 0; i < dv.size(); ++i) {
                            dv[i] *= 1._f + delta;
                            d2v[i] /= 1._f + delta;
                        }
                    } else {
                        for (Size i = 0; i < dv.size(); ++i) {
                            dv[i] = Type(0._f);
                            d2v[i] = Type(0._f);
                        }
                    }
                });*/
    }
};

void AsteroidRotation::setUp() {
    BodySettings bodySettings;
    bodySettings.set(BodySettingsId::ENERGY, 0._f)
        .set(BodySettingsId::ENERGY_RANGE, Range(0._f, INFTY))
        .set(BodySettingsId::PARTICLE_COUNT, 10000)
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e5_f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
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

    solver = makeAuto<DisableDerivativesSolver>(settings, externalForces);

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

    Path outputDir = Path("out") / Path(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
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
    // ArrayView<Float> u = storage->getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> rho = storage->getValue<Float>(QuantityId::DENSITY);
    // ArrayView<TracelessTensor> s = storage->getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    // Interpolation interpol(smaller);
    const Abstract::Eos& eos = dynamic_cast<EosMaterial&>(smaller.getMaterial(0).material()).getEos();

    const Float rho0 = smaller.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
    const Float u0 = smaller.getMaterial(0)->getParam<Float>(BodySettingsId::ENERGY);
    Analytic::StaticSphere sphere(5.e3_f, rho0);
    for (Size i = 0; i < r.size(); ++i) {
        const Float p = sphere.getPressure(getLength(r[i]));
        // u[i] = eos.getInternalEnergy(rho0, p);
        rho[i] = eos.getDensity(p, u0);
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
