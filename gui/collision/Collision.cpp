#include "gui/collision/Collision.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/Uvw.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
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

const Float SCALE = 1.e4_f;

AsteroidCollision::AsteroidCollision() {
    settings.set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::PREDICTOR_CORRECTOR)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-8_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 100._f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::COURANT)
        .set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::NONE)
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1._f)
        .set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 1.e6_f))
        .set(RunSettingsId::SOLVER_FORCES,
            ForceEnum::PRESSURE /*| ForceEnum::SOLID_STRESS*/) //| ForceEnum::GRAVITY) //|
                                                               // ForceEnum::INERTIAL)
        .set(RunSettingsId::SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE)
        .set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD)
        .set(RunSettingsId::SPH_AV_USE_STRESS, false)
        .set(RunSettingsId::SPH_AV_STRESS_FACTOR, 0.04_f)
        .set(RunSettingsId::SPH_AV_ALPHA, 0.15_f)
        .set(RunSettingsId::SPH_AV_BETA, 0.3_f)
        .set(RunSettingsId::SPH_KERNEL, KernelEnum::THOMAS_COUCHMAN)
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
        .set(RunSettingsId::DOMAIN_TYPE, DomainEnum::BLOCK)
        .set(RunSettingsId::DOMAIN_CENTER, Vector(0._f))
        .set(RunSettingsId::DOMAIN_SIZE, Vector(6.05_f, 6.05_f, 6.05_f) * SCALE)
        .set(RunSettingsId::FRAME_ANGULAR_FREQUENCY, Vector(0._f));

    // lhs: kg ms-2
    // rhs: gamma kg  = gamma * kg^2 * m^-3 * ??

    /*settings.set(RunSettingsId::RUN_COMMENT,
        std::string("Homogeneous Gravity with no initial rotation")); // + std::to_string(omega));

    // generate path of the output directory
    std::time_t t = std::time(nullptr);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&t), "%y-%m-%d_%H-%M");
    outputDir = resultsDir / Path(ss.str());
    settings.set(RunSettingsId::RUN_OUTPUT_PATH, (outputDir / "out"_path).native());

    // save code settings to the directory
    settings.saveToFile(outputDir / Path("code.sph"));

    // save the current git commit
    std::string gitSha = getGitCommit(sourceDir).value();
    FileLogger info(outputDir / Path("info.txt"));
    ss.str(""); // clear ss
    ss << std::put_time(std::localtime(&t), "%d.%m.%Y, %H:%M");
    info.write("Run ", runName);
    info.write("Started on ", ss.str());
    info.write("Git commit: ", gitSha);*/
}


void AsteroidCollision::setUp() {
    storage = makeShared<Storage>();
    scheduler = Tbb::getGlobalInstance();

    solver = Factory::getSolver(*scheduler, settings);

    if (wxTheApp->argc > 1) {
        std::string arg(wxTheApp->argv[1]);
        Path path(arg);
        if (!FileSystem::pathExists(path)) {
            wxMessageBox("Cannot locate file " + path.native(), "Error", wxOK);
            return;
        } else {
            BinaryInput input;
            Statistics stats;
            Outcome result = input.load(path, *storage, stats);
            if (!result) {
                wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK);
                return;
            } else {
                // const Float t0 = stats.get<Float>(StatisticsId::RUN_TIME);
                const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
                // const Interval origRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
                // settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(t0, origRange.upper()));
                settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);
            }
        }
    } else {
        Size N = 10000;

        BodySettings body;
        body.set(BodySettingsId::ENERGY, 10._f)
            .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
            .set(BodySettingsId::MELT_ENERGY, 1.e12_f)
            //.set(BodySettingsId::EOS, EosEnum::TILLOTSON)
            .set(BodySettingsId::EOS, EosEnum::TAIT)
            .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
            //.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::DRUCKER_PRAGER)
            .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
            .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
            .set(BodySettingsId::INITIAL_DISTRIBUTION, DistributionEnum::HEXAGONAL)
            .set(BodySettingsId::DRY_FRICTION, 0.8_f)
            .set(BodySettingsId::SURFACE_TENSION, 1.e-12_f)
            .set(BodySettingsId::ENERGY_MIN, 1000._f)
            .set(BodySettingsId::STRESS_TENSOR_MIN, 1.e10_f)
            .set(BodySettingsId::DAMAGE, 1._f)
            .set(BodySettingsId::DAMAGE_MIN, 10._f)
            .set(BodySettingsId::PARTICLE_COUNT, int(N));

        EquationHolder eqs = getStandardEquations(settings);
        eqs += makeExternalForce([](const Vector UNUSED(r)) { return Vector(0._f, -9.81_f, 0._f); });
        // eqs += makeTerm<CohesionTerm>();


        /*FixedParticles::Params fcp;
        fcp.domain =
            makeAuto<BlockDomain>(Vector(4.e5_f, 3.e5_f, 0._f), Vector(8.01e5_f, 6.01e5_f, 3.01e5_f));
        fcp.distribution = Factory::getDistribution(body);
        fcp.material = Factory::getMaterial(body);
        fcp.thickness = 5.e4_f;

        AutoPtr<FixedParticles> bc = makeAuto<FixedParticles>(settings, std::move(fcp));*/

        AutoPtr<IDomain> boundary = Factory::getDomain(settings);
        AutoPtr<GhostParticles> bc = makeAuto<GhostParticles>(std::move(boundary), settings);
        /*bc->setVelocityOverride([](const Vector& r) -> Optional<Vector> {
            if (r[Y] < 1.e3_f) {
                return Vector(0._f); // zero velocity (=> friction)
            } else {
                return NOTHING;
            }
        });*/

        solver = makeAuto<AsymmetricSolver>(*scheduler, settings, eqs); // , std::move(bc));


        //        const Vector dimension = Vector(1._f, 3.2_f, 3._f) * SCALE;
        InitialConditions ic(*scheduler, *solver, settings);

        const Vector dimension = Vector(6._f, 1.5_f, 6._f) * SCALE;
        BlockDomain domain(Vector(0._f), dimension);
        ic.addMonolithicBody(*storage, domain, body);

        SphericalDomain droplet(Vector(0._f, 2._f, 0._f) * SCALE, 0.3_f * SCALE);
        body.set(BodySettingsId::PARTICLE_COUNT, 2000);
        ic.addMonolithicBody(*storage, droplet, body).addVelocity(Vector(0._f, -0.25_f * SCALE, 0._f));

        ASSERT(storage->isValid());

        /*        Presets::CollisionParams params;
                params.targetRadius = 4e5_f;
                params.impactorRadius = 1e5_f;
                params.impactAngle = 30._f * DEG_TO_RAD;
                params.impactSpeed = 5.e3_f;
                params.targetRotation = 0._f;
                params.targetParticleCnt = N;
                params.centerOfMassFrame = true;
                params.optimizeImpactor = true;
                params.body = body;

                solver = Factory::getSolver(*scheduler, settings);
                Presets::Collision data(*scheduler, settings, params);
                data.addTarget(*storage);
                data.addImpactor(*storage);*/
    }

    callbacks = makeAuto<GuiCallbacks>(*controller);

    // add printing of run progres
    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings), settings));
}

void AsteroidCollision::tearDown(const Statistics& UNUSED(stats)) {}


NAMESPACE_SPH_END
