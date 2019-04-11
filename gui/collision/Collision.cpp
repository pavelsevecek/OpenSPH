#include "gui/collision/Collision.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/Uvw.h"
#include "io/FileSystem.h"
#include "io/LogWriter.h"
#include "objects/geometry/Domain.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/Fluids.h"
#include "sph/equations/Potentials.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Presets.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/GravitySolver.h"
#include "sph/solvers/StandardSets.h"
#include "system/Factory.h"
#include "system/Platform.h"
#include "system/Process.h"
#include "system/Profiler.h"
#include "thread/Pool.h"
#include "thread/Tbb.h"
#include <fstream>
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

AsteroidCollision::AsteroidCollision() {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 1.e6_f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::COMPRESSED_FILE)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e6_f))
        .set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_USE_STRESS, true)
        .set(RunSettingsId::SPH_AV_STRESS_FACTOR, 0.06_f)
        .set(RunSettingsId::SPH_AV_ALPHA, 0.15_f)
        .set(RunSettingsId::SPH_AV_BETA, 0.3_f)
        .set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::GRAVITY_SOLVER, GravityEnum::BARNES_HUT)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SPH_KERNEL)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 5._f)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 0.2_f)
        .set(RunSettingsId::TIMESTEPPING_COURANT_NUMBER, 0.2_f)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 100)
        .set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, true)
        .set(RunSettingsId::SPH_SUM_ONLY_UNDAMAGED, false)
        .set(RunSettingsId::DOMAIN_BOUNDARY, BoundaryEnum::GHOST_PARTICLES)
        .set(RunSettingsId::DOMAIN_GHOST_MIN_DIST, 0.25_f)
        .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::HALF_SPACE)
        /*.set(RunSettingsId::DOMAIN_CENTER, Vector(0._f))
        .set(RunSettingsId::DOMAIN_SIZE, Vector(2, 1, 1))*/
        .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));
}


void AsteroidCollision::setUp() {
    storage = makeShared<Storage>();
    scheduler = Factory::getScheduler(settings);
    solver = Factory::getSolver(*scheduler, settings);


    Size N = 100000;

    /*    BodySettings gass;
        gass.set(BodySettingsId::ENERGY, 0._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::PARTICLE_COUNT, int(N));

        BodySettings rock;
        rock.set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
            .set(BodySettingsId::ENERGY_MIN, 1000._f)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e10_f)
            .set(BodySettingsId::DAMAGE_MIN, 10._f);


        const Vector dimension = Vector(2, 1, 1);
        AutoPtr<BlockDomain> domain = makeAuto<BlockDomain>(Vector(0._f), dimension);

        AutoPtr<SphericalDomain> sphere = makeAuto<SphericalDomain>(domain->getCenter(), 0.25_f);

        Array<InitialConditions::BodySetup> bodies;
        bodies.emplaceBack(std::move(sphere), rock);

        InitialConditions ic(*scheduler, *solver, settings);
        Array<BodyView> views = ic.addHeterogeneousBody(
            *storage, InitialConditions::BodySetup(std::move(domain), gass), std::move(bodies));
        views[0].addVelocity(Vector(-10._f, 0._f, 0._f));*/

    BodySettings body;
    body.set(BodySettingsId::DENSITY, 300._f)
        .set(BodySettingsId::SHEAR_MODULUS, 5.e8_f)
        .set(BodySettingsId::EOS, EosEnum::MURNAGHAN)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::ELASTIC)
        .set(BodySettingsId::ENERGY_MIN, 1000._f)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e10_f)
        .set(BodySettingsId::DAMAGE_MIN, 10._f)
        .set(BodySettingsId::PARTICLE_COUNT, int(N));

    EquationHolder eqs = getStandardEquations(settings);
    eqs += makeExternalForce([](const Vector UNUSED(r)) { return Vector(0._f, 0._f, -9.81_f); });

    /*  AutoPtr<IDomain> boundary = Factory::getDomain(settings);
      AutoPtr<GhostParticles> bc = makeAuto<GhostParticles>(std::move(boundary), settings);
      bc->setVelocityOverride([](const Vector& r) -> Optional<Vector> {
          if (r[Y] < 1.e3_f) {
              return Vector(0._f); // zero velocity (=> friction)
          } else {
              return NOTHING;
          }
      });*/

    solver = makeAuto<AsymmetricSolver>(*scheduler, settings, eqs);

    constexpr Float SCALE = 5.e2_f;
    storage = makeShared<Storage>();
    const Vector dimension = Vector(1._f, 1.2_f, 1.8_f) * SCALE;
    const Vector center = Vector(0._f, 0._f, 4._f) * SCALE;
    // BlockDomain block(Vector(0._f, 0._f, 4._f) * SCALE, dimension);
    AffineMatrix tm =
        AffineMatrix::rotateAxis(getNormalized(Vector(0.5_f, 0.9_f, -0.3_f)), 30._f * DEG_TO_RAD);
    TransformedDomain<BlockDomain> block(tm, center, dimension);

    InitialConditions ic(*scheduler, *solver, settings);
    ic.addMonolithicBody(*storage, block, body);

    callbacks = makeAuto<GuiCallbacks>(*controller);

    output = makeAuto<CompressedOutput>(Path("out_%d.scf"), CompressionEnum::NONE);
}

void AsteroidCollision::tearDown(const Statistics& UNUSED(stats)) {}


NAMESPACE_SPH_END
