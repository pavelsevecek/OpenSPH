#include "system/Factory.h"
#include "geometry/Domain.h"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Voxel.h"
#include "physics/Damage.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "solvers/ContinuitySolver.h"
#include "solvers/DensityIndependentSolver.h"
#include "solvers/SummationSolver.h"
#include "sph/Material.h"
#include "sph/boundary/Boundary.h"
#include "sph/initial/Distribution.h"
#include "timestepping/TimeStepCriterion.h"
#include "timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

std::unique_ptr<Abstract::Eos> Factory::getEos(const BodySettings& settings) {
    const EosEnum id = settings.get<EosEnum>(BodySettingsId::EOS);
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return std::make_unique<IdealGasEos>(settings.get<Float>(BodySettingsId::ADIABATIC_INDEX));
    case EosEnum::TILLOTSON:
        return std::make_unique<TillotsonEos>(settings);
    case EosEnum::MURNAGHAN:
        return std::make_unique<MurnaghanEos>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Rheology> Factory::getRheology(const BodySettings& settings) {
    const YieldingEnum id = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    switch (id) {
    case YieldingEnum::NONE:
        return nullptr;
    case YieldingEnum::VON_MISES:
        return std::make_unique<VonMisesRheology>(Factory::getDamage(settings));
    case YieldingEnum::DRUCKER_PRAGER:
        return std::make_unique<DruckerPragerRheology>(Factory::getDamage(settings));
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Damage> Factory::getDamage(const BodySettings& settings) {
    const DamageEnum id = settings.get<DamageEnum>(BodySettingsId::RHEOLOGY_DAMAGE);
    switch (id) {
    case DamageEnum::NONE:
        return std::make_unique<NullDamage>();
    case DamageEnum::SCALAR_GRADY_KIPP:
        /// \todo  where to get kernel radius from??
        return std::make_unique<ScalarDamage>(2._f);
    case DamageEnum::TENSOR_GRADY_KIPP:
        return std::make_unique<TensorDamage>();
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::TimeStepping> Factory::getTimeStepping(const RunSettings& settings,
    const std::shared_ptr<Storage>& storage) {
    const TimesteppingEnum id = settings.get<TimesteppingEnum>(RunSettingsId::TIMESTEPPING_INTEGRATOR);
    switch (id) {
    case TimesteppingEnum::EULER_EXPLICIT:
        return std::make_unique<EulerExplicit>(storage, settings);
    case TimesteppingEnum::PREDICTOR_CORRECTOR:
        return std::make_unique<PredictorCorrector>(storage, settings);
    case TimesteppingEnum::BULIRSCH_STOER:
        return std::make_unique<BulirschStoer>(storage, settings);
    case TimesteppingEnum::RUNGE_KUTTA:
        return std::make_unique<RungeKutta>(storage, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::TimeStepCriterion> Factory::getTimeStepCriterion(const RunSettings& settings) {
    const Size flags = settings.get<int>(RunSettingsId::TIMESTEPPING_CRITERION);
    if (flags == 0) {
        // no criterion
        return nullptr;
    }
    switch (flags) {
    case Size(TimeStepCriterionEnum::COURANT):
        return std::make_unique<CourantCriterion>(settings);
    case Size(TimeStepCriterionEnum::DERIVATIVES):
        return std::make_unique<DerivativeCriterion>(settings);
    case Size(TimeStepCriterionEnum::ACCELERATION):
        return std::make_unique<AccelerationCriterion>();
    default:
        ASSERT(!isPower2(flags)); // multiple criteria, assert in case we add another criterion
        return std::make_unique<MultiCriterion>(settings);
    }
}

std::unique_ptr<Abstract::Finder> Factory::getFinder(const RunSettings& settings) {
    const FinderEnum id = settings.get<FinderEnum>(RunSettingsId::SPH_FINDER);
    switch (id) {
    case FinderEnum::BRUTE_FORCE:
        return std::make_unique<BruteForceFinder>();
    case FinderEnum::KD_TREE:
        return std::make_unique<KdTree>();
    case FinderEnum::VOXEL:
        return std::make_unique<VoxelFinder>();
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Distribution> Factory::getDistribution(const BodySettings& settings) {
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
        return std::make_unique<HexagonalPacking>(flags);
    }
    case DistributionEnum::CUBIC:
        return std::make_unique<CubicPacking>();
    case DistributionEnum::RANDOM:
        return std::make_unique<RandomDistribution>();
    case DistributionEnum::DIEHL_ET_AL:
        return std::make_unique<DiehlEtAlDistribution>([](const Vector&) { return 1._f; });
    case DistributionEnum::LINEAR:
        return std::make_unique<LinearDistribution>();
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Solver> Factory::getSolver(const RunSettings& settings) {
    const SolverEnum id = settings.get<SolverEnum>(RunSettingsId::SOLVER_TYPE);
    switch (id) {
    case SolverEnum::CONTINUITY_SOLVER:
        return std::make_unique<ContinuitySolver>(settings);
    case SolverEnum::SUMMATION_SOLVER:
        return std::make_unique<SummationSolver>(settings);
    case SolverEnum::DENSITY_INDEPENDENT:
        return std::make_unique<DensityIndependentSolver>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Domain> Factory::getDomain(const RunSettings& settings) {
    const DomainEnum id = settings.get<DomainEnum>(RunSettingsId::DOMAIN_TYPE);
    const Vector center = settings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    switch (id) {
    case DomainEnum::NONE:
        return nullptr;
    case DomainEnum::BLOCK:
        return std::make_unique<BlockDomain>(center, settings.get<Vector>(RunSettingsId::DOMAIN_SIZE));
    case DomainEnum::CYLINDER:
        return std::make_unique<CylindricalDomain>(center,
            settings.get<Float>(RunSettingsId::DOMAIN_RADIUS),
            settings.get<Float>(RunSettingsId::DOMAIN_HEIGHT),
            true);
    case DomainEnum::SPHERICAL:
        return std::make_unique<SphericalDomain>(center, settings.get<Float>(RunSettingsId::DOMAIN_RADIUS));
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::BoundaryConditions> Factory::getBoundaryConditions(const RunSettings& settings,
    std::unique_ptr<Abstract::Domain>&& domain) {
    const BoundaryEnum id = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    switch (id) {
    case BoundaryEnum::NONE:
        return nullptr;
    case BoundaryEnum::GHOST_PARTICLES:
        ASSERT(domain != nullptr);
        return std::make_unique<GhostParticles>(std::move(domain), settings);
    case BoundaryEnum::FROZEN_PARTICLES:
        if (domain) {
            const Float radius = settings.get<Float>(RunSettingsId::DOMAIN_FROZEN_DIST);
            return std::make_unique<FrozenParticles>(std::move(domain), radius);
        } else {
            return std::make_unique<FrozenParticles>();
        }
    case BoundaryEnum::WIND_TUNNEL: {
        ASSERT(domain != nullptr);
        const Float radius = settings.get<Float>(RunSettingsId::DOMAIN_FROZEN_DIST);
        return std::make_unique<WindTunnel>(std::move(domain), radius);
    }
    case BoundaryEnum::PROJECT_1D: {
        ASSERT(domain != nullptr);
        const Box box = domain->getBoundingBox();
        return std::make_unique<Projection1D>(Range(box.lower()[X], box.upper()[X]));
    }
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Material> Factory::getMaterial(const BodySettings& settings) {
    const YieldingEnum yieldId = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    const EosEnum eosId = settings.get<EosEnum>(BodySettingsId::EOS);
    switch (yieldId) {
    case YieldingEnum::DRUCKER_PRAGER:
    case YieldingEnum::VON_MISES:
        return std::make_unique<SolidMaterial>(
            settings, Factory::getEos(settings), Factory::getRheology(settings));
    case YieldingEnum::NONE:
        switch (eosId) {
        case EosEnum::NONE:
            return std::make_unique<NullMaterial>(settings);
        default:
            return std::make_unique<EosMaterial>(settings, Factory::getEos(settings));
        }

    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Logger> Factory::getLogger(const RunSettings& settings) {
    const LoggerEnum id = settings.get<LoggerEnum>(RunSettingsId::RUN_LOGGER);
    switch (id) {
    case LoggerEnum::NONE:
        return std::make_unique<DummyLogger>();
    case LoggerEnum::STD_OUT:
        return std::make_unique<StdOutLogger>();
    case LoggerEnum::FILE:
        return std::make_unique<FileLogger>(settings.get<std::string>(RunSettingsId::RUN_LOGGER_FILE));
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Rng> Factory::getRng(const RunSettings& settings) {
    const RngEnum id = settings.get<RngEnum>(RunSettingsId::RUN_RNG);
    const int seed = settings.get<int>(RunSettingsId::RUN_RNG_SEED);
    switch (id) {
    case RngEnum::UNIFORM:
        return std::make_unique<RngWrapper<UniformRng>>(seed);
    case RngEnum::HALTON:
        return std::make_unique<RngWrapper<HaltonQrng>>();
    case RngEnum::BENZ_ASPHAUG:
        return std::make_unique<RngWrapper<BenzAsphaugRng>>(seed);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
