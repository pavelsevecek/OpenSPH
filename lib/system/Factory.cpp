#include "system/Factory.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "gravity/SphericalGravity.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "math/rng/Rng.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/DynamicFinder.h"
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
#include "sph/initial/Distribution.h"
#include "sph/kernel/GravityKernel.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/DensityIndependentSolver.h"
#include "sph/solvers/DifferencedEnergySolver.h"
#include "sph/solvers/GravitySolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SummationSolver.h"
#include "timestepping/TimeStepCriterion.h"
#include "timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

AutoPtr<IEos> Factory::getEos(const BodySettings& settings) {
    const EosEnum id = settings.get<EosEnum>(BodySettingsId::EOS);
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return makeAuto<IdealGasEos>(settings.get<Float>(BodySettingsId::ADIABATIC_INDEX));
    case EosEnum::TAIT:
        return makeAuto<TaitEos>(settings);
    case EosEnum::MIE_GRUNEISEN:
        return makeAuto<MieGruneisenEos>(settings);
    case EosEnum::TILLOTSON:
        return makeAuto<TillotsonEos>(settings);
    case EosEnum::MURNAGHAN:
        return makeAuto<MurnaghanEos>(settings);
    case EosEnum::NONE:
        return nullptr;
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IRheology> Factory::getRheology(const BodySettings& settings) {
    const YieldingEnum id = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    switch (id) {
    case YieldingEnum::NONE:
        return nullptr;
    case YieldingEnum::ELASTIC:
        return makeAuto<ElasticRheology>();
    case YieldingEnum::VON_MISES:
        return makeAuto<VonMisesRheology>(Factory::getDamage(settings));
    case YieldingEnum::DRUCKER_PRAGER:
        return makeAuto<DruckerPragerRheology>(Factory::getDamage(settings));
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IFractureModel> Factory::getDamage(const BodySettings& settings) {
    const FractureEnum id = settings.get<FractureEnum>(BodySettingsId::RHEOLOGY_DAMAGE);
    switch (id) {
    case FractureEnum::NONE:
        return makeAuto<NullFracture>();
    case FractureEnum::SCALAR_GRADY_KIPP:
        /// \todo  where to get kernel radius from??
        return makeAuto<ScalarGradyKippModel>(2._f);
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
    const bool balsara = settings.get<bool>(RunSettingsId::SPH_AV_BALSARA);
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
    case FinderEnum::KD_TREE:
        return makeAuto<KdTree>();
    case FinderEnum::OCTREE:
        NOT_IMPLEMENTED;
        // return makeAuto<Octree>();
    case FinderEnum::UNIFORM_GRID:
        return makeAuto<UniformGridFinder>();
    case FinderEnum::DYNAMIC:
        return makeAuto<DynamicFinder>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IDistribution> Factory::getDistribution(const BodySettings& settings) {
    const DistributionEnum id = settings.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION);
    const bool center = settings.get<bool>(BodySettingsId::CENTER_PARTICLES);
    const bool sort = settings.get<bool>(BodySettingsId::PARTICLE_SORTING);
    const bool sph5mode = settings.get<bool>(BodySettingsId::DISTRIBUTE_MODE_SPH5);
    switch (id) {
    case DistributionEnum::HEXAGONAL: {
        Flags<HexagonalPacking::Options> flags;
        flags.setIf(HexagonalPacking::Options::CENTER, center || sph5mode);
        flags.setIf(HexagonalPacking::Options::SORTED, sort);
        flags.setIf(HexagonalPacking::Options::SPH5_COMPATIBILITY, sph5mode);
        return makeAuto<HexagonalPacking>(flags);
    }
    case DistributionEnum::CUBIC:
        return makeAuto<CubicPacking>();
    case DistributionEnum::RANDOM:
        /// \todo user-selected seed?
        return makeAuto<RandomDistribution>(1234);
    case DistributionEnum::DIEHL_ET_AL: {
        const Float strength = settings.get<Float>(BodySettingsId::DIELH_STRENGTH);
        const Size maxDiff = settings.get<int>(BodySettingsId::DIEHL_MAX_DIFFERENCE);
        return makeAuto<DiehlDistribution>([](const Vector&) { return 1._f; }, maxDiff, 50, strength);
    }
    case DistributionEnum::LINEAR:
        return makeAuto<LinearDistribution>();
    default:
        NOT_IMPLEMENTED;
    }
}

template <typename TSolver>
static AutoPtr<ISolver> getActualSolver(const RunSettings& settings, EquationHolder&& eqs) {
    const Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
    if (forces.has(ForceEnum::GRAVITY)) {
        return makeAuto<GravitySolver<TSolver>>(settings, std::move(eqs));
    } else {
        return makeAuto<TSolver>(settings, std::move(eqs));
    }
}

AutoPtr<ISolver> Factory::getSolver(const RunSettings& settings) {
    EquationHolder eqs = getStandardEquations(settings);
    const SolverEnum id = settings.get<SolverEnum>(RunSettingsId::SOLVER_TYPE);
    auto throwIfGravity = [&settings] {
        const Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
        if (forces.has(ForceEnum::GRAVITY)) {
            throw InvalidSetup("Using solver incompatible with gravity.");
        }
    };
    switch (id) {
    case SolverEnum::SYMMETRIC_SOLVER:
        return getActualSolver<SymmetricSolver>(settings, std::move(eqs));
    case SolverEnum::ASYMMETRIC_SOLVER:
        return getActualSolver<AsymmetricSolver>(settings, std::move(eqs));
    case SolverEnum::SUMMATION_SOLVER:
        throwIfGravity();
        return makeAuto<SummationSolver>(settings);
    case SolverEnum::DENSITY_INDEPENDENT:
        throwIfGravity();
        return makeAuto<DensityIndependentSolver>(settings);
    case SolverEnum::DIFFERENED_ENERGY:
        throwIfGravity();
        return makeAuto<DifferencedEnergySolver>(settings, std::move(eqs));
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

    switch (id) {
    case GravityEnum::SPHERICAL:
        return makeAuto<SphericalGravity>();
    case GravityEnum::BRUTE_FORCE:
        return makeAuto<BruteForceGravity>(std::move(kernel));
    case GravityEnum::BARNES_HUT: {
        const Float theta = settings.get<Float>(RunSettingsId::GRAVITY_OPENING_ANGLE);
        const MultipoleOrder order =
            MultipoleOrder(settings.get<int>(RunSettingsId::GRAVITY_MULTIPOLE_ORDER));
        const int leafSize = settings.get<int>(RunSettingsId::GRAVITY_LEAF_SIZE);
        return makeAuto<BarnesHut>(theta, order, std::move(kernel), leafSize);
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ICollisionHandler> Factory::getCollisionHandler(const RunSettings& settings) {
    const CollisionHandlerEnum id = settings.get<CollisionHandlerEnum>(RunSettingsId::COLLISION_HANDLER);
    switch (id) {
    case CollisionHandlerEnum::ELASTIC_BOUNCE:
        return makeAuto<ElasticBounceHandler>(settings);
    case CollisionHandlerEnum::PERFECT_MERGING:
        return makeAuto<MergingCollisionHandler>(0._f);
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
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IBoundaryCondition> Factory::getBoundaryConditions(const RunSettings& settings) {
    const BoundaryEnum id = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    AutoPtr<IDomain> domain = getDomain(settings);
    switch (id) {
    case BoundaryEnum::NONE:
        struct NoBoundaryCondition : public IBoundaryCondition {
            virtual void initialize(Storage& UNUSED(storage)) override {}
            virtual void finalize(Storage& UNUSED(storage)) override {}
        };

        return makeAuto<NoBoundaryCondition>();
    case BoundaryEnum::GHOST_PARTICLES:
        ASSERT(domain != nullptr);
        return makeAuto<GhostParticles>(std::move(domain), settings);
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
    case BoundaryEnum::PROJECT_1D: {
        ASSERT(domain != nullptr);
        const Box box = domain->getBoundingBox();
        return makeAuto<Projection1D>(Interval(box.lower()[X], box.upper()[X]));
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IMaterial> Factory::getMaterial(const BodySettings& settings) {
    const YieldingEnum yieldId = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    const EosEnum eosId = settings.get<EosEnum>(BodySettingsId::EOS);
    switch (yieldId) {
    case YieldingEnum::DRUCKER_PRAGER:
    case YieldingEnum::VON_MISES:
    case YieldingEnum::ELASTIC:
        return makeAuto<SolidMaterial>(settings);
    case YieldingEnum::NONE:
        switch (eosId) {
        case EosEnum::NONE:
            return makeAuto<NullMaterial>(settings);
        default:
            return makeAuto<EosMaterial>(settings);
        }

    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ILogger> Factory::getLogger(const RunSettings& settings) {
    const LoggerEnum id = settings.get<LoggerEnum>(RunSettingsId::RUN_LOGGER);
    switch (id) {
    case LoggerEnum::NONE:
        return makeAuto<DummyLogger>();
    case LoggerEnum::STD_OUT:
        return makeAuto<StdOutLogger>();
    case LoggerEnum::FILE: {
        const Path path(settings.get<std::string>(RunSettingsId::RUN_LOGGER_FILE));
        return makeAuto<FileLogger>(path);
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IOutput> Factory::getOutput(const RunSettings& settings) {
    const OutputEnum id = settings.get<OutputEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
    const Path outputPath(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_PATH));
    const Path fileMask(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_NAME));
    switch (id) {
    case OutputEnum::NONE:
        return makeAuto<NullOutput>();
    case OutputEnum::TEXT_FILE:
        return makeAuto<TextOutput>(outputPath / fileMask,
            settings.get<std::string>(RunSettingsId::RUN_NAME),
            settings.getFlags<OutputQuantityFlag>(RunSettingsId::RUN_OUTPUT_QUANTITIES));
    case OutputEnum::BINARY_FILE:
        return makeAuto<BinaryOutput>(outputPath / fileMask);
    case OutputEnum::PKDGRAV_INPUT: {
        PkdgravParams pkd;
        pkd.omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
        return makeAuto<PkdgravOutput>(outputPath / fileMask, std::move(pkd));
    }
    default:
        NOT_IMPLEMENTED;
    }
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
        return CoreTriangle();
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
