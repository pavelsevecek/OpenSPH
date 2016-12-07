#include "system/Factory.h"
#include "geometry/Domain.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "physics/Eos.h"
#include "sph/boundary/Boundary.h"
#include "sph/distributions/Distribution.h"
#include "sph/timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

std::unique_ptr<Abstract::Eos> Factory::getEos(const Settings<BodySettingsIds>& settings) {
    const EosEnum id = settings.get<EosEnum>(BodySettingsIds::EOS);
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return std::make_unique<IdealGasEos>(settings.get<float>(BodySettingsIds::ADIABATIC_INDEX));
    case EosEnum::TILLOTSON:
        return std::make_unique<TillotsonEos>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::TimeStepping> Factory::getTimestepping(const Settings<GlobalSettingsIds>& settings,
                                                                 const std::shared_ptr<Storage>& storage) {
    const TimesteppingEnum id = settings.get<TimesteppingEnum>(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR);
    switch (id) {
    case TimesteppingEnum::EULER_EXPLICIT:
        return std::make_unique<EulerExplicit>(storage, settings);
    case TimesteppingEnum::PREDICTOR_CORRECTOR:
        return std::make_unique<PredictorCorrector>(storage, settings);
    case TimesteppingEnum::BULIRSCH_STOER:
        return std::make_unique<BulirschStoer>(storage, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Finder> Factory::getFinder(const Settings<GlobalSettingsIds>& settings) {
    const FinderEnum id = settings.get<FinderEnum>(GlobalSettingsIds::SPH_FINDER);
    switch (id) {
    case FinderEnum::BRUTE_FORCE:
        return std::make_unique<BruteForceFinder>();
    case FinderEnum::KD_TREE:
        return std::make_unique<KdTree>();
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::Distribution> Factory::getDistribution(const Settings<BodySettingsIds>& settings) {
    const DistributionEnum id = settings.get<DistributionEnum>(BodySettingsIds::INITIAL_DISTRIBUTION);
    switch (id) {
    case DistributionEnum::HEXAGONAL:
        return std::make_unique<HexagonalPacking>();
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

std::unique_ptr<Abstract::Domain> Factory::getDomain(const Settings<GlobalSettingsIds>& settings) {
    const DomainEnum id = settings.get<DomainEnum>(GlobalSettingsIds::DOMAIN_TYPE);
    const Vector center = settings.get<Vector>(GlobalSettingsIds::DOMAIN_CENTER);
    switch (id) {
    case DomainEnum::NONE:
        return nullptr;
    case DomainEnum::BLOCK:
        return std::make_unique<BlockDomain>(center, settings.get<Vector>(GlobalSettingsIds::DOMAIN_SIZE));
    case DomainEnum::SPHERICAL:
        return std::make_unique<SphericalDomain>(center,
                                                 settings.get<Float>(GlobalSettingsIds::DOMAIN_RADIUS));
    default:
        NOT_IMPLEMENTED;
    }
}

std::unique_ptr<Abstract::BoundaryConditions> Factory::getBoundaryConditions(
    const Settings<GlobalSettingsIds>& settings,
    std::unique_ptr<Abstract::Domain>&& domain) {
    const BoundaryEnum id = settings.get<BoundaryEnum>(GlobalSettingsIds::DOMAIN_BOUNDARY);
    switch (id) {
    case BoundaryEnum::NONE:
        return nullptr;
    case BoundaryEnum::DOMAIN_PROJECTING:
        ASSERT(domain != nullptr);
        return std::make_unique<DomainProjecting>(std::move(domain), ProjectingOptions::ZERO_PERPENDICULAR);
    case BoundaryEnum::PROJECT_1D: {
        ASSERT(domain != nullptr);
        const Vector center = domain->getCenter();
        const Float radius  = domain->getBoundingRadius();
        return std::make_unique<Projection1D>(Range(center[0] - radius, center[0] + radius));
    }
    default:
        NOT_IMPLEMENTED;
    }
}


NAMESPACE_SPH_END
