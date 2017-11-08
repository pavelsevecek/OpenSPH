#include "system/Factory.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "io/Logger.h"
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
#include "sph/solvers/ContinuitySolver.h"
#include "sph/solvers/DensityIndependentSolver.h"
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
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IRheology> Factory::getRheology(const BodySettings& settings) {
    const YieldingEnum id = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    switch (id) {
    case YieldingEnum::NONE:
        return makeAuto<ElasticRheology>();
    case YieldingEnum::VON_MISES:
        return makeAuto<VonMisesRheology>(Factory::getDamage(settings));
    case YieldingEnum::DRUCKER_PRAGER:
        return makeAuto<DruckerPragerRheology>(Factory::getDamage(settings));
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IDamage> Factory::getDamage(const BodySettings& settings) {
    const DamageEnum id = settings.get<DamageEnum>(BodySettingsId::RHEOLOGY_DAMAGE);
    switch (id) {
    case DamageEnum::NONE:
        return makeAuto<NullDamage>();
    case DamageEnum::SCALAR_GRADY_KIPP:
        /// \todo  where to get kernel radius from??
        return makeAuto<ScalarDamage>(2._f);
    case DamageEnum::TENSOR_GRADY_KIPP:
        return makeAuto<TensorDamage>();
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
    case TimesteppingEnum::RUNGE_KUTTA:
        return makeAuto<RungeKutta>(storage, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ITimeStepCriterion> Factory::getTimeStepCriterion(const RunSettings& settings) {
    const Size flags = settings.get<int>(RunSettingsId::TIMESTEPPING_CRITERION);
    if (flags == 0) {
        // no criterion
        return nullptr;
    }
    switch (flags) {
    case Size(TimeStepCriterionEnum::COURANT):
        return makeAuto<CourantCriterion>(settings);
    case Size(TimeStepCriterionEnum::DERIVATIVES):
        return makeAuto<DerivativeCriterion>(settings);
    case Size(TimeStepCriterionEnum::ACCELERATION):
        return makeAuto<AccelerationCriterion>(settings);
    default:
        ASSERT(!isPower2(flags)); // multiple criteria, assert in case we add another criterion
        return makeAuto<MultiCriterion>(settings);
    }
}

AutoPtr<INeighbourFinder> Factory::getFinder(const RunSettings& settings) {
    const FinderEnum id = settings.get<FinderEnum>(RunSettingsId::SPH_FINDER);
    switch (id) {
    case FinderEnum::BRUTE_FORCE:
        return makeAuto<BruteForceFinder>();
    case FinderEnum::KD_TREE:
        return makeAuto<KdTree>();
    case FinderEnum::OCTREE:
        return makeAuto<Octree>();
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
        return makeAuto<RandomDistribution>();
    case DistributionEnum::DIEHL_ET_AL:
        return makeAuto<DiehlDistribution>([](const Vector&) { return 1._f; });
    case DistributionEnum::LINEAR:
        return makeAuto<LinearDistribution>();
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<ISolver> Factory::getSolver(const RunSettings& settings) {
    const SolverEnum id = settings.get<SolverEnum>(RunSettingsId::SOLVER_TYPE);
    switch (id) {
    case SolverEnum::CONTINUITY_SOLVER:
        return makeAuto<ContinuitySolver>(settings);
    case SolverEnum::SUMMATION_SOLVER:
        return makeAuto<SummationSolver>(settings);
    case SolverEnum::DENSITY_INDEPENDENT:
        return makeAuto<DensityIndependentSolver>(settings);
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
    default:
        NOT_IMPLEMENTED;
    }

    switch (id) {
    case GravityEnum::BRUTE_FORCE:
        return makeAuto<BruteForceGravity>(std::move(kernel));
    case GravityEnum::BARNES_HUT: {
        const Float theta = settings.get<Float>(RunSettingsId::GRAVITY_OPENING_ANGLE);
        const MultipoleOrder order = settings.get<MultipoleOrder>(RunSettingsId::GRAVITY_MULTIPOLE_ORDER);
        const int leafSize = settings.get<int>(RunSettingsId::GRAVITY_LEAF_SIZE);
        return makeAuto<BarnesHut>(theta, order, std::move(kernel), leafSize);
    }
    case GravityEnum::VOXEL:
        NOT_IMPLEMENTED;
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

AutoPtr<IBoundaryCondition> Factory::getBoundaryConditions(const RunSettings& settings,
    AutoPtr<IDomain>&& domain) {
    const BoundaryEnum id = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    switch (id) {
    case BoundaryEnum::NONE:
        return nullptr;
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
        return makeAuto<SolidMaterial>(settings, Factory::getEos(settings), Factory::getRheology(settings));
    case YieldingEnum::NONE:
        switch (eosId) {
        case EosEnum::NONE:
            return makeAuto<NullMaterial>(settings);
        default:
            return makeAuto<EosMaterial>(settings, Factory::getEos(settings));
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

NAMESPACE_SPH_END
