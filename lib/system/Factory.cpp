#include "system/Factory.h"
#include "geometry/Domain.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Voxel.h"
#include "physics/Eos.h"
#include "sph/boundary/Boundary.h"
#include "sph/initial/Distribution.h"
#include "sph/timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

std::unique_ptr<Abstract::Eos> Factory::getEos(const BodySettings& settings) {
    const EosEnum id = settings.get<EosEnum>(BodySettingsIds::EOS);
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return std::make_unique<IdealGasEos>(settings.get<Float>(BodySettingsIds::ADIABATIC_INDEX));
    case EosEnum::TILLOTSON:
        return std::make_unique<TillotsonEos>(settings);
    case EosEnum::MURNAGHAN:
        return std::make_unique<MurnaghanEos>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::TimeStepping> Factory::getTimestepping(const GlobalSettings& settings,
    const std::shared_ptr<Storage>& storage) {
    const TimesteppingEnum id = settings.get<TimesteppingEnum>(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR);
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

std::unique_ptr<Abstract::Finder> Factory::getFinder(const GlobalSettings& settings) {
    const FinderEnum id = settings.get<FinderEnum>(GlobalSettingsIds::SPH_FINDER);
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
    const DistributionEnum id = settings.get<DistributionEnum>(BodySettingsIds::INITIAL_DISTRIBUTION);
    switch (id) {
    case DistributionEnum::HEXAGONAL:
        return std::make_unique<HexagonalPacking>(HexagonalPacking::Options::CENTER);
    case DistributionEnum::CUBIC:
        return std::make_unique<CubicPacking>();
    case DistributionEnum::RANDOM:
        return std::make_unique<RandomDistribution>();
    /*case DistributionEnum::DIEHL_ET_AL:
        return std::make_unique<DiehlDistribution>();*/
    case DistributionEnum::LINEAR:
        return std::make_unique<LinearDistribution>();
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Domain> Factory::getDomain(const GlobalSettings& settings) {
    const DomainEnum id = settings.get<DomainEnum>(GlobalSettingsIds::DOMAIN_TYPE);
    const Vector center = settings.get<Vector>(GlobalSettingsIds::DOMAIN_CENTER);
    switch (id) {
    case DomainEnum::NONE:
        return nullptr;
    case DomainEnum::BLOCK:
        return std::make_unique<BlockDomain>(center, settings.get<Vector>(GlobalSettingsIds::DOMAIN_SIZE));
    case DomainEnum::CYLINDER:
        return std::make_unique<CylindricalDomain>(center,
            settings.get<Float>(GlobalSettingsIds::DOMAIN_RADIUS),
            settings.get<Float>(GlobalSettingsIds::DOMAIN_HEIGHT),
            false);
    case DomainEnum::SPHERICAL:
        return std::make_unique<SphericalDomain>(
            center, settings.get<Float>(GlobalSettingsIds::DOMAIN_RADIUS));
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::BoundaryConditions> Factory::getBoundaryConditions(const GlobalSettings& settings,
    std::unique_ptr<Abstract::Domain>&& domain) {
    const BoundaryEnum id = settings.get<BoundaryEnum>(GlobalSettingsIds::DOMAIN_BOUNDARY);
    switch (id) {
    case BoundaryEnum::NONE:
        return nullptr;
    case BoundaryEnum::GHOST_PARTICLES:
        ASSERT(domain != nullptr);
        return std::make_unique<GhostParticles>(std::move(domain), settings);
    case BoundaryEnum::DOMAIN_PROJECTING:
        ASSERT(domain != nullptr);
        return std::make_unique<DomainProjecting>(std::move(domain), ProjectingOptions::ZERO_PERPENDICULAR);
    case BoundaryEnum::PROJECT_1D: {
        ASSERT(domain != nullptr);
        const Vector center = domain->getCenter();
        const Float radius = domain->getBoundingRadius();
        return std::make_unique<Projection1D>(Range(center[0] - radius, center[0] + radius));
    }
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Logger> Factory::getLogger(const GlobalSettings& settings) {
    const LoggerEnum id = settings.get<LoggerEnum>(GlobalSettingsIds::RUN_LOGGER);
    switch (id) {
    case LoggerEnum::NO_LOGGER:
        return std::make_unique<DummyLogger>();
    case LoggerEnum::STD_OUT:
        return std::make_unique<StdOutLogger>();
    case LoggerEnum::FILE:
        return std::make_unique<FileLogger>(settings.get<std::string>(GlobalSettingsIds::RUN_LOGGER_FILE));
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
