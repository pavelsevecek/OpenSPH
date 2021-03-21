#include "system/Factory.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/CachedGravity.h"
#include "gravity/Collision.h"
#include "gravity/SphericalGravity.h"
#include "gravity/SymmetricGravity.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "math/rng/Rng.h"
#include "objects/Exceptions.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/HashMapFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Octree.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "physics/Damage.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "sph/Materials.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/av/Balsara.h"
#include "sph/equations/av/MorrisMonaghan.h"
#include "sph/equations/av/Riemann.h"
#include "sph/equations/av/Standard.h"
#include "sph/initial/Distribution.h"
#include "sph/kernel/GravityKernel.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/DensityIndependentSolver.h"
#include "sph/solvers/ElasticDeformationSolver.h"
#include "sph/solvers/EnergyConservingSolver.h"
#include "sph/solvers/GravitySolver.h"
#include "sph/solvers/SimpleSolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SummationSolver.h"
#include "thread/OpenMp.h"
#include "thread/Pool.h"
#include "thread/Tbb.h"
#include "timestepping/TimeStepCriterion.h"
#include "timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

AutoPtr<IEos> Factory::getEos(const BodySettings& body) {
    const EosEnum id = body.get<EosEnum>(BodySettingsId::EOS);
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return makeAuto<IdealGasEos>(body.get<Float>(BodySettingsId::ADIABATIC_INDEX));
    case EosEnum::TAIT:
        return makeAuto<TaitEos>(body);
    case EosEnum::MIE_GRUNEISEN:
        return makeAuto<MieGruneisenEos>(body);
    case EosEnum::TILLOTSON:
        return makeAuto<TillotsonEos>(body);
    case EosEnum::SIMPLIFIED_TILLOTSON:
        return makeAuto<SimplifiedTillotsonEos>(body);
    case EosEnum::MURNAGHAN:
        return makeAuto<MurnaghanEos>(body);
    case EosEnum::NONE:
        return nullptr;
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IRheology> Factory::getRheology(const BodySettings& body) {
    const YieldingEnum id = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    switch (id) {
    case YieldingEnum::NONE:
        return nullptr;
    case YieldingEnum::ELASTIC:
        return makeAuto<ElasticRheology>();
    case YieldingEnum::VON_MISES:
        return makeAuto<VonMisesRheology>(Factory::getDamage(body));
    case YieldingEnum::DRUCKER_PRAGER:
        return makeAuto<DruckerPragerRheology>(Factory::getDamage(body));
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IFractureModel> Factory::getDamage(const BodySettings& body) {
    const FractureEnum id = body.get<FractureEnum>(BodySettingsId::RHEOLOGY_DAMAGE);
    switch (id) {
    case FractureEnum::NONE:
        return makeAuto<NullFracture>();
    case FractureEnum::SCALAR_GRADY_KIPP:
        return makeAuto<ScalarGradyKippModel>();
    case FractureEnum::TENSOR_GRADY_KIPP:
        return makeAuto<TensorGradyKippModel>();
    default:
        NOT_IMPLEMENTED;
    }
}

template <typename T>
AutoPtr<IEquationTerm> makeAV(const RunSettings& settings, const bool balsara) {
    if (balsara) {
        return makeAuto<BalsaraSwitch<T>>(settings);
    } else {
        return makeAuto<T>();
    }
}

AutoPtr<IEquationTerm> Factory::getArtificialViscosity(const RunSettings& settings) {
    const ArtificialViscosityEnum id = settings.get<ArtificialViscosityEnum>(RunSettingsId::SPH_AV_TYPE);
    const bool balsara = settings.get<bool>(RunSettingsId::SPH_AV_USE_BALSARA);
    switch (id) {
    case ArtificialViscosityEnum::NONE:
        return nullptr;
    case ArtificialViscosityEnum::STANDARD:
        return makeAV<StandardAV>(settings, balsara);
    case ArtificialViscosityEnum::RIEMANN:
        return makeAV<RiemannAV>(settings, balsara);
    case ArtificialViscosityEnum::MORRIS_MONAGHAN:
        return makeAV<MorrisMonaghanAV>(settings, balsara);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ITimeStepping> Factory::getTimeStepping(const RunSettings& settings,
    const SharedPtr<Storage>& storage) {
    const TimesteppingEnum id = settings.get<TimesteppingEnum>(RunSettingsId::TIMESTEPPING_INTEGRATOR);
    switch (id) {
    case TimesteppingEnum::EULER_EXPLICIT:
        return makeAuto<EulerExplicit>(storage, settings);
    case TimesteppingEnum::PREDICTOR_CORRECTOR:
        return makeAuto<PredictorCorrector>(storage, settings);
    case TimesteppingEnum::LEAP_FROG:
        return makeAuto<LeapFrog>(storage, settings);
    case TimesteppingEnum::BULIRSCH_STOER:
        return makeAuto<BulirschStoer>(storage, settings);
    case TimesteppingEnum::MODIFIED_MIDPOINT:
        return makeAuto<ModifiedMidpointMethod>(storage, settings);
    case TimesteppingEnum::RUNGE_KUTTA:
        return makeAuto<RungeKutta>(storage, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ITimeStepCriterion> Factory::getTimeStepCriterion(const RunSettings& settings) {
    const Flags<TimeStepCriterionEnum> flags =
        settings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
    if (flags == EMPTY_FLAGS) {
        // no criterion
        return nullptr;
    }
    return makeAuto<MultiCriterion>(settings);
}

AutoPtr<ISymmetricFinder> Factory::getFinder(const RunSettings& settings) {
    const FinderEnum id = settings.get<FinderEnum>(RunSettingsId::SPH_FINDER);
    switch (id) {
    case FinderEnum::BRUTE_FORCE:
        return makeAuto<BruteForceFinder>();
    case FinderEnum::KD_TREE: {
        const Size leafSize = settings.get<int>(RunSettingsId::FINDER_LEAF_SIZE);
        const Size maxDepth = settings.get<int>(RunSettingsId::FINDER_MAX_PARALLEL_DEPTH);
        return makeAuto<KdTree<KdNode>>(leafSize, maxDepth);
    }
    case FinderEnum::OCTREE:
        NOT_IMPLEMENTED;
        // return makeAuto<Octree>();
    case FinderEnum::UNIFORM_GRID:
        return makeAuto<UniformGridFinder>();
    case FinderEnum::HASH_MAP:
        return makeAuto<HashMapFinder>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

SharedPtr<IScheduler> Factory::getScheduler(const RunSettings& settings) {
    const Size threadCnt = settings.get<int>(RunSettingsId::RUN_THREAD_CNT);
    const Size granularity = settings.get<int>(RunSettingsId::RUN_THREAD_GRANULARITY);
    if (threadCnt == 1) {
        // optimization - use directly SequentialScheduler instead of thread pool with 1 thread
        return SequentialScheduler::getGlobalInstance();
    } else {
#ifdef SPH_USE_TBB
        SharedPtr<Tbb> scheduler = Tbb::getGlobalInstance();
        scheduler->setGranularity(granularity);
        return scheduler;
#elif SPH_USE_OPENMP
        SharedPtr<OmpScheduler> scheduler = OmpScheduler::getGlobalInstance();
        scheduler->setGranularity(granularity);
        return scheduler;
#else
        static WeakPtr<ThreadPool> weakGlobal = ThreadPool::getGlobalInstance();
        if (SharedPtr<ThreadPool> global = weakGlobal.lock()) {
            if (global->getThreadCnt() == threadCnt) {
                // scheduler is already used by some component and has the same thread count, we can reuse the
                // instance instead of creating a new one
                global->setGranularity(granularity);
                return global;
            }
        }

        SharedPtr<ThreadPool> newPool = makeShared<ThreadPool>(threadCnt, granularity);
        weakGlobal = newPool;
        return newPool;
#endif
    }
}

AutoPtr<IDistribution> Factory::getDistribution(const BodySettings& body,
    Function<bool(Float)> progressCallback) {
    const DistributionEnum id = body.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION);
    const bool center = body.get<bool>(BodySettingsId::CENTER_PARTICLES);
    const bool sort = body.get<bool>(BodySettingsId::PARTICLE_SORTING);
    const bool sph5mode = body.get<bool>(BodySettingsId::DISTRIBUTE_MODE_SPH5);
    switch (id) {
    case DistributionEnum::HEXAGONAL: {
        Flags<HexagonalPacking::Options> flags;
        flags.setIf(HexagonalPacking::Options::CENTER, center || sph5mode);
        flags.setIf(HexagonalPacking::Options::SORTED, sort);
        flags.setIf(HexagonalPacking::Options::SPH5_COMPATIBILITY, sph5mode);
        return makeAuto<HexagonalPacking>(flags, progressCallback);
    }
    case DistributionEnum::CUBIC:
        return makeAuto<CubicPacking>();
    case DistributionEnum::RANDOM:
        /// \todo user-selected seed?
        return makeAuto<RandomDistribution>(1234);
    case DistributionEnum::STRATIFIED:
        return makeAuto<StratifiedDistribution>(1234);
    case DistributionEnum::DIEHL_ET_AL: {
        DiehlParams diehl;
        diehl.particleDensity = [](const Vector&) { return 1._f; };
        diehl.strength = body.get<Float>(BodySettingsId::DIEHL_STRENGTH);
        diehl.maxDifference = body.get<int>(BodySettingsId::DIEHL_MAX_DIFFERENCE);
        return makeAuto<DiehlDistribution>(diehl);
    }
    case DistributionEnum::PARAMETRIZED_SPIRALING:
        return makeAuto<ParametrizedSpiralingDistribution>(1234);
    case DistributionEnum::LINEAR:
        return makeAuto<LinearDistribution>();
    default:
        NOT_IMPLEMENTED;
    }
}

template <typename TSolver>
static AutoPtr<ISolver> getActualSolver(IScheduler& scheduler,
    const RunSettings& settings,
    EquationHolder&& eqs,
    AutoPtr<IBoundaryCondition>&& bc) {
    const Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
    if (forces.has(ForceEnum::SELF_GRAVITY)) {
        return makeAuto<GravitySolver<TSolver>>(scheduler, settings, std::move(eqs), std::move(bc));
    } else {
        return makeAuto<TSolver>(scheduler, settings, std::move(eqs), std::move(bc));
    }
}

AutoPtr<ISolver> Factory::getSolver(IScheduler& scheduler, const RunSettings& settings) {
    return getSolver(scheduler, settings, makeAuto<NullBoundaryCondition>());
}

AutoPtr<ISolver> Factory::getSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IBoundaryCondition>&& bc) {
    return getSolver(scheduler, settings, std::move(bc), {});
}

AutoPtr<ISolver> Factory::getSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IBoundaryCondition>&& bc,
    const EquationHolder& additionalTerms) {
    EquationHolder eqs = getStandardEquations(settings, additionalTerms);
    const SolverEnum id = settings.get<SolverEnum>(RunSettingsId::SPH_SOLVER_TYPE);
    auto throwIfGravity = [&settings] {
        const Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
        if (forces.has(ForceEnum::SELF_GRAVITY)) {
            throw InvalidSetup("Using solver incompatible with gravity.");
        }
    };
    switch (id) {
    case SolverEnum::SYMMETRIC_SOLVER:
        return getActualSolver<SymmetricSolver<DIMENSIONS>>(
            scheduler, settings, std::move(eqs), std::move(bc));
    case SolverEnum::ASYMMETRIC_SOLVER:
        return getActualSolver<AsymmetricSolver>(scheduler, settings, std::move(eqs), std::move(bc));
    case SolverEnum::ENERGY_CONSERVING_SOLVER:
        return getActualSolver<EnergyConservingSolver>(scheduler, settings, std::move(eqs), std::move(bc));
    case SolverEnum::ELASTIC_DEFORMATION_SOLVER:
        throwIfGravity();
        return makeAuto<ElasticDeformationSolver>(scheduler, settings, std::move(bc));
    case SolverEnum::SUMMATION_SOLVER:
        throwIfGravity();
        return makeAuto<SummationSolver<DIMENSIONS>>(scheduler, settings);
    case SolverEnum::DENSITY_INDEPENDENT:
        throwIfGravity();
        return makeAuto<DensityIndependentSolver>(scheduler, settings);
    case SolverEnum::SIMPLE_SOLVER:
        throwIfGravity();
        return makeAuto<SimpleSolver>(scheduler, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IGravity> Factory::getGravity(const RunSettings& settings) {
    const GravityEnum id = settings.get<GravityEnum>(RunSettingsId::GRAVITY_SOLVER);
    const GravityKernelEnum kernelId = settings.get<GravityKernelEnum>(RunSettingsId::GRAVITY_KERNEL);
    GravityLutKernel kernel;
    switch (kernelId) {
    case GravityKernelEnum::POINT_PARTICLES:
        // do nothing, keep default-constructed kernel
        break;
    case GravityKernelEnum::SPH_KERNEL:
        kernel = Factory::getGravityKernel(settings);
        break;
    case GravityKernelEnum::SOLID_SPHERES:
        kernel = SolidSphereKernel();
        break;
    default:
        NOT_IMPLEMENTED;
    }

    AutoPtr<IGravity> gravity;
    switch (id) {
    case GravityEnum::SPHERICAL:
        gravity = makeAuto<SphericalGravity>();
        break;
    case GravityEnum::BRUTE_FORCE:
        gravity = makeAuto<BruteForceGravity>(std::move(kernel));
        break;
    case GravityEnum::BARNES_HUT: {
        const Float theta = settings.get<Float>(RunSettingsId::GRAVITY_OPENING_ANGLE);
        const MultipoleOrder order =
            MultipoleOrder(settings.get<int>(RunSettingsId::GRAVITY_MULTIPOLE_ORDER));
        const Size leafSize = settings.get<int>(RunSettingsId::FINDER_LEAF_SIZE);
        const Size maxDepth = settings.get<int>(RunSettingsId::FINDER_MAX_PARALLEL_DEPTH);
        const Float constant = settings.get<Float>(RunSettingsId::GRAVITY_CONSTANT);
        gravity = makeAuto<BarnesHut>(theta, order, std::move(kernel), leafSize, maxDepth, constant);
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }

    // wrap gravity in case of symmetric boundary conditions
    if (settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY) == BoundaryEnum::SYMMETRIC) {
        gravity = makeAuto<SymmetricGravity>(std::move(gravity));
    }

    // wrap if gravity recomputation period is specified
    const Float period = settings.get<Float>(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD);
    if (period > 0._f) {
        gravity = makeAuto<CachedGravity>(period, std::move(gravity));
    }

    return gravity;
}

AutoPtr<ICollisionHandler> Factory::getCollisionHandler(const RunSettings& settings) {
    const CollisionHandlerEnum id = settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER);
    switch (id) {
    case CollisionHandlerEnum::ELASTIC_BOUNCE:
        return makeAuto<ElasticBounceHandler>(settings);
    case CollisionHandlerEnum::PERFECT_MERGING:
        return makeAuto<MergingCollisionHandler>(0._f, 0._f);
    case CollisionHandlerEnum::MERGE_OR_BOUNCE:
        return makeAuto<FallbackHandler<MergingCollisionHandler, ElasticBounceHandler>>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IOverlapHandler> Factory::getOverlapHandler(const RunSettings& settings) {
    const OverlapEnum id = settings.get<OverlapEnum>(RunSettingsId::COLLISION_OVERLAP);
    switch (id) {
    case OverlapEnum::NONE:
        return makeAuto<NullOverlapHandler>();
    case OverlapEnum::FORCE_MERGE:
        return makeAuto<MergeOverlapHandler>();
    case OverlapEnum::REPEL:
        return makeAuto<RepelHandler<ElasticBounceHandler>>(settings);
    case OverlapEnum::REPEL_OR_MERGE:
        using FollowupHandler = FallbackHandler<MergingCollisionHandler, ElasticBounceHandler>;
        return makeAuto<RepelHandler<FollowupHandler>>(settings);
    case OverlapEnum::INTERNAL_BOUNCE:
        return makeAuto<InternalBounceHandler>(settings);
    case OverlapEnum::PASS_OR_MERGE:
        return makeAuto<MergeBoundHandler>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IDomain> Factory::getDomain(const RunSettings& settings) {
    const DomainEnum id = settings.get<DomainEnum>(RunSettingsId::DOMAIN_TYPE);
    const Vector center = settings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    switch (id) {
    case DomainEnum::NONE:
        return nullptr;
    case DomainEnum::BLOCK:
        return makeAuto<BlockDomain>(center, settings.get<Vector>(RunSettingsId::DOMAIN_SIZE));
    case DomainEnum::CYLINDER:
        return makeAuto<CylindricalDomain>(center,
            settings.get<Float>(RunSettingsId::DOMAIN_RADIUS),
            settings.get<Float>(RunSettingsId::DOMAIN_HEIGHT),
            true);
    case DomainEnum::SPHERICAL:
        return makeAuto<SphericalDomain>(center, settings.get<Float>(RunSettingsId::DOMAIN_RADIUS));
    case DomainEnum::ELLIPSOIDAL:
        return makeAuto<EllipsoidalDomain>(center, settings.get<Vector>(RunSettingsId::DOMAIN_SIZE));
    case DomainEnum::HALF_SPACE:
        return makeAuto<HalfSpaceDomain>();
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IDomain> Factory::getDomain(const BodySettings& settings) {
    const DomainEnum id = settings.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
    const Vector center = settings.get<Vector>(BodySettingsId::BODY_CENTER);
    switch (id) {
    case DomainEnum::SPHERICAL:
        return makeAuto<SphericalDomain>(center, settings.get<Float>(BodySettingsId::BODY_RADIUS));
    case DomainEnum::BLOCK:
        return makeAuto<BlockDomain>(center, settings.get<Vector>(BodySettingsId::BODY_DIMENSIONS));
    case DomainEnum::CYLINDER:
        return makeAuto<CylindricalDomain>(center,
            settings.get<Float>(BodySettingsId::BODY_RADIUS),
            settings.get<Float>(BodySettingsId::BODY_HEIGHT),
            true);
    case DomainEnum::ELLIPSOIDAL:
        return makeAuto<EllipsoidalDomain>(center, settings.get<Vector>(BodySettingsId::BODY_DIMENSIONS));
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IBoundaryCondition> Factory::getBoundaryConditions(const RunSettings& settings,
    SharedPtr<IDomain> domain) {
    const BoundaryEnum id =
        domain ? settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY) : BoundaryEnum::NONE;

    switch (id) {
    case BoundaryEnum::NONE:
        return makeAuto<NullBoundaryCondition>();
    case BoundaryEnum::GHOST_PARTICLES:
        ASSERT(domain != nullptr);
        return makeAuto<GhostParticles>(std::move(domain), settings);
    case BoundaryEnum::FIXED_PARTICLES:
        throw InvalidSetup("FixedParticles cannot be create from Factory, use manual setup.");
    case BoundaryEnum::FROZEN_PARTICLES:
        if (domain) {
            const Float radius = settings.get<Float>(RunSettingsId::DOMAIN_FROZEN_DIST);
            return makeAuto<FrozenParticles>(std::move(domain), radius);
        } else {
            return makeAuto<FrozenParticles>();
        }
    case BoundaryEnum::WIND_TUNNEL: {
        ASSERT(domain != nullptr);
        const Float radius = settings.get<Float>(RunSettingsId::DOMAIN_FROZEN_DIST);
        return makeAuto<WindTunnel>(std::move(domain), radius);
    }
    case BoundaryEnum::PERIODIC: {
        const Box box = domain->getBoundingBox();
        return makeAuto<PeriodicBoundary>(box);
    }
    case BoundaryEnum::SYMMETRIC:
        return makeAuto<SymmetricBoundary>();
    case BoundaryEnum::KILL_ESCAPERS:
        ASSERT(domain != nullptr);
        return makeAuto<KillEscapersBoundary>(domain);
    case BoundaryEnum::PROJECT_1D: {
        ASSERT(domain != nullptr);
        const Box box = domain->getBoundingBox();
        return makeAuto<Projection1D>(Interval(box.lower()[X], box.upper()[X]));
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IBoundaryCondition> Factory::getBoundaryConditions(const RunSettings& settings) {
    return getBoundaryConditions(settings, Factory::getDomain(settings));
}

AutoPtr<IMaterial> Factory::getMaterial(const BodySettings& body) {
    const YieldingEnum yieldId = body.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    const EosEnum eosId = body.get<EosEnum>(BodySettingsId::EOS);
    switch (yieldId) {
    case YieldingEnum::DRUCKER_PRAGER:
    case YieldingEnum::VON_MISES:
    case YieldingEnum::ELASTIC:
        return makeAuto<SolidMaterial>(body);
    case YieldingEnum::NONE:
        switch (eosId) {
        case EosEnum::NONE:
            return makeAuto<NullMaterial>(body);
        default:
            return makeAuto<EosMaterial>(body);
        }

    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ILogger> Factory::getLogger(const RunSettings& settings) {
    const LoggerEnum id = settings.get<LoggerEnum>(RunSettingsId::RUN_LOGGER);
    switch (id) {
    case LoggerEnum::NONE:
        return makeAuto<NullLogger>();
    case LoggerEnum::STD_OUT:
        return makeAuto<StdOutLogger>();
    case LoggerEnum::FILE: {
        const Path path(settings.get<std::string>(RunSettingsId::RUN_LOGGER_FILE));
        const Flags<FileLogger::Options> flags =
            FileLogger::Options::ADD_TIMESTAMP | FileLogger::Options::KEEP_OPENED;
        return makeAuto<FileLogger>(path, flags);
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ILogWriter> Factory::getLogWriter(SharedPtr<ILogger> logger, const RunSettings& settings) {
    const int verbosity = clamp(settings.get<int>(RunSettingsId::RUN_LOGGER_VERBOSITY), 0, 3);
    switch (verbosity) {
    case 0:
        return makeAuto<NullLogWriter>();
    case 1:
        return makeAuto<BriefLogWriter>(logger, settings);
    case 2:
        return makeAuto<StandardLogWriter>(logger, settings);
    case 3:
        return makeAuto<VerboseLogWriter>(logger, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IOutput> Factory::getOutput(const RunSettings& settings) {
    const IoEnum id = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
    const Path outputPath(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_PATH));
    const Path fileMask(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
    const Size firstIndex = settings.get<int>(RunSettingsId::RUN_OUTPUT_FIRST_INDEX);
    const OutputFile file(outputPath / fileMask, firstIndex);
    switch (id) {
    case IoEnum::NONE:
        return makeAuto<NullOutput>();
    case IoEnum::TEXT_FILE: {
        const std::string name = settings.get<std::string>(RunSettingsId::RUN_NAME);
        const Flags<OutputQuantityFlag> flags =
            settings.getFlags<OutputQuantityFlag>(RunSettingsId::RUN_OUTPUT_QUANTITIES);
        return makeAuto<TextOutput>(file, name, flags);
    }
    case IoEnum::BINARY_FILE: {
        const RunTypeEnum runType = settings.get<RunTypeEnum>(RunSettingsId::RUN_TYPE);
        return makeAuto<BinaryOutput>(file, runType);
    }
    case IoEnum::COMPRESSED_FILE: {
        const RunTypeEnum runType = settings.get<RunTypeEnum>(RunSettingsId::RUN_TYPE);
        return makeAuto<CompressedOutput>(file, CompressionEnum::NONE /*TODO*/, runType);
    }
    case IoEnum::VTK_FILE: {
        const Flags<OutputQuantityFlag> flags =
            settings.getFlags<OutputQuantityFlag>(RunSettingsId::RUN_OUTPUT_QUANTITIES);
        return makeAuto<VtkOutput>(file, flags);
    }
    case IoEnum::PKDGRAV_INPUT: {
        PkdgravParams pkd;
        pkd.omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        return makeAuto<PkdgravOutput>(file, std::move(pkd));
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IInput> Factory::getInput(const Path& path) {
    const std::string ext = path.extension().native();
    if (ext == "ssf") {
        return makeAuto<BinaryInput>();
    } else if (ext == "scf") {
        return makeAuto<CompressedInput>();
    } else if (ext == "h5") {
        return makeAuto<Hdf5Input>();
    } else if (ext == "tab") {
        return makeAuto<TabInput>();
    } else {
        if (ext.size() > 3 && ext.substr(ext.size() - 3) == ".bt") {
            return makeAuto<PkdgravInput>();
        }
    }
    throw InvalidSetup("Unknown file type: " + path.native());
}

AutoPtr<IRng> Factory::getRng(const RunSettings& settings) {
    const RngEnum id = settings.get<RngEnum>(RunSettingsId::RUN_RNG);
    const int seed = settings.get<int>(RunSettingsId::RUN_RNG_SEED);
    switch (id) {
    case RngEnum::UNIFORM:
        return makeAuto<RngWrapper<UniformRng>>(seed);
    case RngEnum::HALTON:
        return makeAuto<RngWrapper<HaltonQrng>>();
    case RngEnum::BENZ_ASPHAUG:
        return makeAuto<RngWrapper<BenzAsphaugRng>>(seed);
    default:
        NOT_IMPLEMENTED;
    }
}

template <Size D>
LutKernel<D> Factory::getKernel(const RunSettings& settings) {
    const KernelEnum id = settings.get<KernelEnum>(RunSettingsId::SPH_KERNEL);
    switch (id) {
    case KernelEnum::CUBIC_SPLINE:
        return CubicSpline<D>();
    case KernelEnum::FOURTH_ORDER_SPLINE:
        return FourthOrderSpline<D>();
    case KernelEnum::GAUSSIAN:
        return Gaussian<D>();
    case KernelEnum::TRIANGLE:
        return TriangleKernel<D>();
    case KernelEnum::CORE_TRIANGLE:
        ASSERT(D == 3);
        // original core triangle has radius 1, rescale to 2 for drop-in replacement of cubic spline
        return ScalingKernel<3, CoreTriangle>(2._f);
    case KernelEnum::THOMAS_COUCHMAN:
        return ThomasCouchmanKernel<D>();
    case KernelEnum::WENDLAND_C2:
        ASSERT(D == 3);
        return WendlandC2();
    case KernelEnum::WENDLAND_C4:
        ASSERT(D == 3);
        return WendlandC4();
    case KernelEnum::WENDLAND_C6:
        ASSERT(D == 3);
        return WendlandC6();
    default:
        NOT_IMPLEMENTED;
    }
}

template LutKernel<1> Factory::getKernel(const RunSettings& settings);
template LutKernel<2> Factory::getKernel(const RunSettings& settings);
template LutKernel<3> Factory::getKernel(const RunSettings& settings);

GravityLutKernel Factory::getGravityKernel(const RunSettings& settings) {
    const KernelEnum id = settings.get<KernelEnum>(RunSettingsId::SPH_KERNEL);
    switch (id) {
    case KernelEnum::CUBIC_SPLINE:
        return GravityKernel<CubicSpline<3>>();
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
